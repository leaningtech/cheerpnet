#include "cheerpnet.h"
#include "utils.h"

#include <cheerp/client.h>

namespace [[cheerp::genericjs]] client
{
	struct FirebasePortData: public Object
	{
		cheerpnet::Port get_port();
		void set_port(cheerpnet::Port);
	};
	struct DataChannel: public EventTarget
	{
		void send(String*);
		void send(Uint8Array&);
		void set_binaryType(const String&);
		void close();
	};
	struct ChannelEvent: public Event
	{
		DataChannel* get_channel();
	};
	struct ConnectionData
	{
		DataChannel* get_channel();
	};
}

namespace [[cheerp::genericjs]]
{
	enum ConnectionState
	{
		INVALID,
		CONNECTING,
		READY,
		CLOSED,
	};
	struct ConnectionData
	{
		client::String* peerKey{nullptr};
		client::DataChannel* channel{nullptr};
		client::RTCPeerConnection* conn{nullptr};
		ConnectionState state{INVALID};
		utils::Vector<client::Uint8Array*> outQueue{};
	};
	struct IncomingPacket
	{
		cheerpnet::AddrInfo addr;
		client::Uint8Array* buf;
	};
	struct SocketData
	{
		cheerpnet::Port port{0};
		client::FirebaseReference* portRef{nullptr};
		utils::Vector<IncomingPacket> inQueue{};
	};
	client::RTCConfiguration* iceConf = nullptr;
	utils::Vector<SocketData> sockets;
	client::TMap<int, int>* ports = new client::TMap<int, int>();
	utils::Vector<ConnectionData> connections;
	cheerpnet::Callback recvCb;
	client::FirebaseDatabase* database = nullptr;
	static bool paused = false;

	void catchPromise(client::Promise* p, client::EventListener* e)
	{
		asm("%0.catch(%1)" : : "r"(p), "r"(e));
	}
}
namespace [[cheerp::genericjs]] cheerpnet
{
	constexpr Address baseAddr = 0x0a000002;
	static Address idx_to_addr(int idx)
	{
		return idx + baseAddr;
	}
	static int addr_to_idx(Address addr)
	{
		return addr - baseAddr;
	}
	static bool valid_fd(SocketFD fd)
	{
		return fd >= 0 && fd < sockets.size();
	}
	static bool valid_addr(Address a)
	{
		int i = addr_to_idx(a);
		return i >= 0 && i < connections.size();
	}
	void init(client::FirebaseConfig* fb, client::RTCConfiguration* ice)
	{
		client::firebase.initializeApp(fb);
		iceConf = ice;
		database = client::firebase.database();
	}
	client::String* local_key()
	{
		static client::String* key = nullptr;
		if (key == nullptr)
		{
			key = database->ref("/peers")->push()->get_key();
		}
		return key;
	}
	static int random_int(int min, int max)
	{
		return client::Math.random() * (max - min) + min;
	}
	static void allocate_port(SocketData& s)
	{
		while (s.port == 0)
		{
			Port candidate = random_int(10000, 1<<16);
			if (!ports->has(candidate))
			{
				s.port = candidate;
				break;
			}
		}
		ports->set(s.port, &s - &sockets[0]);
	}
	static void close_connection(ConnectionData& c)
	{
		if (c.channel)
		{
			c.channel->close();
			c.channel = nullptr;
		}
		if (c.conn)
		{
			c.conn->close();
			c.conn = nullptr;
		}
		c.state = ConnectionState::INVALID;
	}
	static void flush(ConnectionData& c)
	{
		// TODO do this more efficiently
		while (!c.outQueue.isEmpty())
		{
			c.channel->send(*c.outQueue[0]);
			c.outQueue.erase(&c.outQueue[0]);
		}
	}
	static void dispatch_packet(ConnectionData& c, client::ArrayBuffer* data)
	{
		client::Uint8Array& buf = *(new client::Uint8Array(data));
		AddrInfo addr;
		addr.addr = idx_to_addr(&c - &connections[0]);
		addr.port = (buf[0] << 8) | buf[1];
		Port local_port = (buf[2] << 8) | buf[3];
		if (!ports->has(local_port))
			return;
		SocketData& s = sockets[ports->get(local_port)];
		s.inQueue.pushBack(IncomingPacket{addr, &buf});
		if (recvCb && !paused)
			recvCb();
	}

	static client::DataChannel* getDataChannel(client::RTCPeerConnection* conn)
	{
		client::DataChannel* chan;
		asm("%1.createDataChannel('chan', {ordered: false, maxRetransmits: 0})" : "=r"(chan) : "r"(conn));
		return chan;
	}
	static int connect_handshake(ConnectionData& c)
	{
		assert(database != nullptr);
		assert(c.state == ConnectionState::INVALID || c.state == ConnectionState::CLOSED);
		c.state = ConnectionState::CONNECTING;
		database->ref("/peers")->child(c.peerKey)
			->once("value")
			->then(cheerp::Callback([&c](client::FirebaseSnapshot* snapshot)
		{
			auto* obj = snapshot->val<client::Object>();
			if (obj == nullptr)
			{
				c.state = ConnectionState::INVALID;
				return;
			}
			c.conn = new client::RTCPeerConnection(iceConf);
			c.channel = getDataChannel(c.conn);
			c.channel->set_binaryType("arraybuffer");
			c.channel->addEventListener("message", cheerp::Callback([&c](client::MessageEvent* e)
			{
				client::ArrayBuffer* data = (client::ArrayBuffer*)e->get_data();
				dispatch_packet(c, data);
			}));
			c.conn->createOffer()->then(cheerp::Callback([&c](client::RTCSessionDescriptionInit* o)
			{
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				c.conn->setLocalDescription(o);
				c.conn->set_onicecandidate(cheerp::Callback([&c, o, candidates](client::RTCPeerConnectionIceEvent* e)
				{
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
					auto* incomingRef = database->ref("/peers")->child(c.peerKey)->child("incoming")->child(local_key());
					auto* offer = client::JSON.stringify(o);
					client::String* accept = new client::String();
					incomingRef->set(CHEERP_OBJECT(offer, candidates, accept));
					incomingRef
						->child("accept")
						->on("value", cheerp::Callback([&c, incomingRef](client::FirebaseSnapshot* snapshot)
					{
						auto* obj = snapshot->val<client::Object>();
						if (obj == new client::String())
							return;
						incomingRef->child("accept")->off("value");
						auto* answer = static_cast<client::String*>((*obj)["answer"]);
						auto* a = static_cast<client::RTCSessionDescriptionInit*>(client::JSON.parse(*answer));
						auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
						c.conn->setRemoteDescription(a);
						for (int i = 0; i < candidates->get_length(); ++i)
						{
							catchPromise(c.conn->addIceCandidate((*candidates)[i]), cheerp::Callback([](){}));
						}
						incomingRef->remove();
						c.channel->addEventListener("open", cheerp::Callback([&c]()
						{
							c.state = ConnectionState::READY;
							flush(c);
						}));
					}));
				}));
			}));
		}));
		return 0;
	}

	static int listen_handshake()
	{
		assert(database != nullptr);
		static bool listening = false;
		if (listening)
			return 0;
		listening = true;
		database->ref("/peers")->child(local_key())->child("incoming")
			->on("child_added", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			auto* incomingRef = snapshot->get_ref();
			ConnectionData& c = *connections.pushBack(ConnectionData());
			auto* obj = snapshot->val<client::Object>();

			auto* offer = static_cast<client::String*>((*obj)["offer"]);
			auto* o = static_cast<client::RTCSessionDescriptionInit*>(client::JSON.parse(*offer));
			auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
			c.peerKey = snapshot->get_key();
			c.state = ConnectionState::CONNECTING;
			c.conn = new client::RTCPeerConnection(iceConf);
			c.conn->setRemoteDescription(o);
			for (int i = 0; i < candidates->get_length(); ++i)
			{
				catchPromise(c.conn->addIceCandidate((*candidates)[i]), cheerp::Callback([](){}));
			}
			c.conn->createAnswer()
				->then(cheerp::Callback([&c, incomingRef](client::RTCSessionDescriptionInit* a)
			{
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				c.conn->setLocalDescription(a);

				c.conn->set_onicecandidate(cheerp::Callback([&c, a, candidates, incomingRef](client::RTCPeerConnectionIceEvent* e)
				{
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
					auto* answer = client::JSON.stringify(a);
					incomingRef->child("accept")->update(CHEERP_OBJECT(answer, candidates));

					c.conn->addEventListener("datachannel", cheerp::Callback([&c](client::ChannelEvent* e)
					{
						c.channel = e->get_channel();
						c.channel->set_binaryType("arraybuffer");
						c.channel->addEventListener("message", cheerp::Callback([&c](client::MessageEvent* e)
						{
							client::ArrayBuffer* data = (client::ArrayBuffer*)e->get_data();
							dispatch_packet(c, data);
						}));
						c.channel->addEventListener("open", cheerp::Callback([&c]()
						{
							c.state = ConnectionState::READY;
							flush(c);
						}));
					}));

				}));
			}));
		}));
		return 0;
	}

	static void publish_port(SocketData& s)
	{
		auto* key = local_key();
		if (s.portRef == nullptr)
			s.portRef = database->ref("/peers")->child(key)->child("ports")->push();
		auto* portData = new client::FirebasePortData();
		portData->set_port(s.port);
		s.portRef->set(portData);
	}
	static void unpublish_port(SocketData& s)
	{
		if (s.portRef != nullptr)
		{
			s.portRef->remove();
		}
	}
	int bind(SocketFD fd, AddrInfo* addr)
	{
		if (database == nullptr)
			return -1;
		if (!valid_fd(fd))
			return -1;
		if (addr->addr != 0 && addr->addr != baseAddr-1)
			return -1;
		SocketData& s = sockets[fd];
		if (s.port != 0)
		{
			ports->delete_(s.port);
		}
		s.port = addr->port;
		allocate_port(s);
		publish_port(s);
		listen_handshake();
		return 0;
	}


	int sendto(SocketFD fd, uint8_t* buf, int len, const AddrInfo* addr)
	{
		assert(len < 16*1024);
		if (!valid_fd(fd))
			return -1;
		if (!valid_addr(addr->addr))
			return -1;
		SocketData& s = sockets[fd];
		ConnectionData& c = connections[addr_to_idx(addr->addr)];
		allocate_port(s);

		client::Uint8Array* tbuf = cheerp::MakeTypedArray(buf, len);
		client::Uint8Array& newBuf = *(new client::Uint8Array(len+4));
		newBuf[0] = (s.port >> 8) & 0xff;
		newBuf[1] = (s.port) & 0xff;
		newBuf[2] = (addr->port >> 8) & 0xff;
		newBuf[3] = (addr->port) & 0xff;
		newBuf.set(tbuf, 4);

		if (c.state == ConnectionState::READY)
		{
			assert(c.channel != nullptr);
			asm("try{");
			c.channel->send(newBuf);
			asm("}catch{");
			close_connection(c);
			asm("return -1;}");
			return len;
		}
		if (c.state != ConnectionState::CONNECTING)
		{
			connect_handshake(c);
		}
		c.outQueue.pushBack(&newBuf);
		return len;
	}
	int recvfrom(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr)
	{
		if (!valid_fd(fd))
			return -1;
		SocketData& s = sockets[fd];
		if (s.portRef == nullptr)
		{
			AddrInfo addr { 0, 0 };
			bind(fd, &addr);
		}
		if (s.inQueue.size() == 0)
			return 0;
		*addr = s.inQueue[0].addr;
		client::Uint8Array& recvBuf = *s.inQueue[0].buf;
		int bufLen = recvBuf.get_length()-4;
		int toRead = len <= bufLen ? len : bufLen;
		for(int i=0;i<toRead;i++)
			buf[i] = recvBuf[i+4];
		s.inQueue.erase(&s.inQueue[0]);

		return toRead;
	}
	int recvCallback(Callback cb)
	{
		recvCb = cheerp::move(cb);
		return 0;
	}
	SocketFD socket()
	{
		SocketFD fd = sockets.size();
		sockets.pushBack(SocketData());
		return fd;
	}
	int close(SocketFD fd)
	{
		if (!valid_fd(fd))
			return -1;
		SocketData& s = sockets[fd];
		if (s.portRef)
			s.portRef->remove();
		ports->delete_(s.port);
		unpublish_port(s);
		s.portRef = nullptr;
		s.port = 0;
		return 0;
	}
	Address resolve(client::String* key)
	{
		for (int i = 0; i < connections.size(); ++i)
		{
			if (connections[i].peerKey == key)
				return idx_to_addr(i);
		}
		connections.pushBack(ConnectionData());
		connections[connections.size()-1].peerKey = key;
		return idx_to_addr(connections.size()-1);
	}
	client::String* reverseResolve(Address addr)
	{
		if (!valid_addr(addr))
			return nullptr;
		return connections[addr_to_idx(addr)].peerKey;
	}
	void suspend()
	{
		paused = true;
		for (int i = 0; i < sockets.size(); ++i)
		{
			unpublish_port(sockets[i]);
		}
	}
	void resume()
	{
		paused = false;
		for (int i = 0; i < sockets.size(); ++i)
		{
			if (sockets[i].portRef != nullptr)
				publish_port(sockets[i]);
		}
		// TODO do it only if packets queued
		if (recvCb)
			recvCb();
	}
}

extern "C"
{
using namespace cheerpnet;

[[cheerp::genericjs]]
int cheerpNetBind(int fd, Address addr, Port port)
{
	AddrInfo info{addr, port};
	return bind(fd, &info);
}
[[cheerp::genericjs]]
int cheerpNetSendTo(int fd, uint8_t* buf, int len, Address addr, Port port)
{
	AddrInfo info{addr, port};
	return sendto(fd, buf, len, &info);
}
[[cheerp::genericjs]]
int cheerpNetRecvFrom(int fd, uint8_t* buf, int len, Address* addr, Port* port)
{
	AddrInfo info;
	int ret = recvfrom(fd, buf, len, &info);
	*addr = info.addr;
	*port = info.port;
	return ret;
}
[[cheerp::genericjs]]
int cheerpNetRecvCallback(CheerpNetCallback cb)
{
	return recvCallback(cb);
}
[[cheerp::genericjs]]
uint32_t cheerpNetResolve(const char* name)
{
	return resolve(new client::String(name));
}
[[cheerp::genericjs]]
int cheerpNetSocket()
{
	return socket();
}
[[cheerp::genericjs]]
int cheerpNetClose(SocketFD fd)
{
	return close(fd);
}

}
