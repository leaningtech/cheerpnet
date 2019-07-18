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
		void set_onopen(client::EventListener*);
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
	client::TMap<cheerpnet::Port, int>* ports = new client::TMap<cheerpnet::Port, int>();
	utils::Vector<ConnectionData> connections;
	cheerpnet::Callback recvCb{nullptr};
	client::FirebaseDatabase* database = nullptr;
}
namespace [[cheerp::genericjs]] cheerpnet
{
	constexpr Address baseAddr = 0x0a00002;
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
		if (recvCb != nullptr)
			reinterpret_cast<void(*)(void)>(recvCb)();
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
			c.conn->createOffer()->then(cheerp::Callback([&c](client::RTCSessionDescription* o)
			{
				auto* offer = static_cast<client::RTCSessionDescriptionInit*>(o->toJSON());
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				c.conn->setLocalDescription(offer);
				c.conn->set_onicecandidate(cheerp::Callback([&c, offer, candidates](client::RTCPeerConnectionIceEvent* e)
				{
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
					auto* incomingRef = database->ref("/peers")->child(c.peerKey)->child("incoming")->child(local_key());
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
						auto* answer = static_cast<client::RTCSessionDescriptionInit*>((*obj)["answer"]);
						auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
						c.conn->setRemoteDescription(answer);
						for (int i = 0; i < candidates->get_length(); ++i)
						{
							c.conn->addIceCandidate((*candidates)[i]);
						}
						incomingRef->remove();
						c.channel->set_onopen(cheerp::Callback([&c]()
						{
							c.state = ConnectionState::READY;
							// TODO do this more efficiently
							while (!c.outQueue.isEmpty())
							{
								c.channel->send(*c.outQueue[0]);
								c.outQueue.erase(&c.outQueue[0]);
							}
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
			auto* offer = static_cast<client::RTCSessionDescriptionInit*>((*obj)["offer"]);
			auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
			c.peerKey = snapshot->get_key();
			c.state = ConnectionState::CONNECTING;
			c.conn = new client::RTCPeerConnection(iceConf);
			c.conn->setRemoteDescription(offer);
			for (int i = 0; i < candidates->get_length(); ++i)
			{
				c.conn->addIceCandidate((*candidates)[i]);
			}
			c.conn->createAnswer()
				->then(cheerp::Callback([&c, incomingRef](client::RTCSessionDescriptionInit* answer)
			{
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				c.conn->setLocalDescription(answer);

				c.conn->set_onicecandidate(cheerp::Callback([&c, answer, candidates, incomingRef](client::RTCPeerConnectionIceEvent* e)
				{
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
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
						c.state = ConnectionState::READY;
						// TODO do this more efficiently
						while (!c.outQueue.isEmpty())
						{
							c.channel->send(*c.outQueue[0]);
							c.outQueue.erase(&c.outQueue[0]);
						}
					}));

				}));
			}));
		}));
		return 0;
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
		if (s.portRef != nullptr)
		{
			ports->delete_(s.port);
			s.portRef->remove();
		}
		s.port = addr->port;
		auto* key = local_key();
		auto* portRef = database->ref("/peers")->child(key)->child("ports")->push();
		auto* portData = new client::FirebasePortData();
		portData->set_port(s.port);
		portRef->set(portData);
		s.portRef = portRef;
		allocate_port(s);
		listen_handshake();
		return 0;
	}


	int sendto(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr)
	{
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
			c.channel->send(newBuf);
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
		recvCb = cb;
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
}

extern "C"
{


}