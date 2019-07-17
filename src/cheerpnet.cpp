#include "cheerpnet.h"
#include "utils.h"

#include <cheerp/client.h>

namespace [[cheerp::genericjs]] client
{
	struct FirebaseReference;
	struct FirebaseDatabase
	{
		FirebaseReference* ref(const client::String&);
	};
	struct Firebase
	{
		void initializeApp(FirebaseConfig*);
		FirebaseDatabase* database();
	};
	extern Firebase& firebase;
	struct FirebaseReference
	{
		void on(const String&, EventListener*);
		void off(const String&, EventListener*);
		void off(const String&);
		Promise* once(const String&);
		void set(Object*);
		void update(Object*);
		void remove();
		FirebaseReference* push();
		FirebaseReference* child(const String&);
		FirebaseReference* orderByChild(const String&);
		FirebaseReference* startAt(double);
		FirebaseReference* endAt(double);
		String* get_key();
	};
	struct FirebaseSnapshot
	{
		template<class T>
		T* val();
		String* get_key();
		void forEach(client::EventListener* e);
		FirebaseReference* get_ref();
	};
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
	enum SocketState
	{
		INVALID,
		READY,
		LISTENING,
		CONNECTING,
		CLOSED,
	};
	struct SocketData
	{
		client::String* peerKey{nullptr};
		client::DataChannel* channel{nullptr};
		client::RTCPeerConnection* conn{nullptr};
		SocketState state{INVALID};
		cheerpnet::Port port{0};
		client::FirebaseReference* portRef{nullptr};
		utils::Vector<client::Uint8Array*> readQueue{};
		cheerpnet::Callback recvCb{nullptr};
	};
	client::RTCConfiguration* iceConf = nullptr;
	utils::Vector<SocketData> sockets;
	client::FirebaseDatabase* database = nullptr;
	utils::Vector<client::String*> addrToKeys;
}
namespace [[cheerp::genericjs]] cheerpnet
{
	constexpr Address baseAddr = 0x0a00001;
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
	static bool ready_fd(SocketFD fd)
	{
		return valid_fd(fd) && sockets[fd].state == SocketState::READY;
	}
	static bool valid_addr(Address a)
	{
		int i = addr_to_idx(a);
		return i >= 0 && i < addrToKeys.size() && addrToKeys[i] != nullptr;
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

	int listen(SocketFD fd, Port port)
	{
		if (database == nullptr)
			return -1;
		if (!valid_fd(fd))
			return -1;
		SocketData& s = sockets[fd];
		if (s.state != SocketState::INVALID)
			return -1;
		s.port = port;
		auto* key = local_key();
		auto* portRef = database->ref("/peers")->child(key)->push();
		auto* portData = new client::FirebasePortData();
		portData->set_port(port);
		portRef->set(portData);
		s.portRef = portRef;
		s.state = SocketState::LISTENING;
		return 0;
	}

	static client::DataChannel* getDataChannel(client::RTCPeerConnection* conn)
	{
		client::DataChannel* chan;
		asm("%1.createDataChannel('chan', {ordered: false, maxRetransmits: 0})" : "=r"(chan) : "r"(conn));
		return chan;
	}
	static void connect_handshake(Connection* conn, Callback cb)
	{
		SocketData& s = sockets[conn->fd];
		s.state = SocketState::CONNECTING;
		s.conn = new client::RTCPeerConnection(iceConf);
		s.channel = getDataChannel(s.conn);
		s.channel->set_binaryType("arraybuffer");
		s.channel->addEventListener("message", cheerp::Callback([conn](client::MessageEvent* e)
		{
			SocketData& s = sockets[conn->fd];
			client::ArrayBuffer* data = (client::ArrayBuffer*)e->get_data();
			s.readQueue.pushBack(new client::Uint8Array(data));
			if (s.recvCb != nullptr)
				reinterpret_cast<void(*)(Connection*)>(s.recvCb)(conn);
		}));
		s.conn->createOffer()->then(cheerp::Callback([conn, cb](client::RTCSessionDescriptionInit* offer)
		{
			SocketData& s = sockets[conn->fd];
			auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
			s.conn->setLocalDescription(offer);
			s.conn->set_onicecandidate(cheerp::Callback([conn, offer, candidates, cb](client::RTCPeerConnectionIceEvent* e)
			{
				SocketData& s = sockets[conn->fd];
				if (e->get_candidate() != nullptr)
				{
					candidates->push(e->get_candidate()->toJSON());
					return;
				}
				auto* incomingRef = s.portRef->child("incoming")->push();
				client::String* accept = new client::String();
				static Port nextPort = 12000; // TODO keep an array of used ports
				Port port = conn->local_port != 0 ? conn->local_port : nextPort++;
				conn->local_port = port;
				incomingRef->set(CHEERP_OBJECT(offer, candidates, port, accept));
				incomingRef
					->child("accept")
					->on("value", cheerp::Callback([conn, incomingRef, cb](client::FirebaseSnapshot* snapshot)
				{
					SocketData& s = sockets[conn->fd];
					auto* obj = snapshot->val<client::Object>();
					if (obj == new client::String())
						return;
					incomingRef->child("accept")->off("value");
					auto* answer = static_cast<client::RTCSessionDescriptionInit*>((*obj)["answer"]);
					auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
					s.conn->setRemoteDescription(answer);
					for (int i = 0; i < candidates->get_length(); ++i)
					{
						s.conn->addIceCandidate((*candidates)[i]);
					}
					s.state = SocketState::READY;
					incomingRef->remove();
					s.channel->set_onopen(cheerp::Callback([conn, cb]()
					{
						reinterpret_cast<void(*)(Connection*)>(cb)(conn);
					}));
				}));
			}));
		}));
	}
	int connect(Connection* conn, Callback cb)
	{
		if (database == nullptr)
			return -1;
		if (!valid_fd(conn->fd))
			return -1;
		if (!valid_addr(conn->remote_addr))
			return -1;
		client::String* peerKey = addrToKeys[addr_to_idx(conn->remote_addr)];
		database->ref("/peers")->child(peerKey)
			->once("value")
			->then(cheerp::Callback([conn, cb, peerKey](client::FirebaseSnapshot* snapshot)
		{
			auto* obj = snapshot->val<client::Object>();
			auto* keys = (client::TArray<client::String>*)client::Object::keys(obj);
			for (int i = 0; i < keys->get_length(); ++i)
			{
				client::String* key = (*keys)[i];
				auto* value = (client::FirebasePortData*)(*obj)[*key];
				if (value->get_port() == conn->remote_port)
				{
					sockets[conn->fd].peerKey = peerKey;
					sockets[conn->fd].portRef = database->ref("/peers")->child(peerKey)->child(key);
					connect_handshake(conn, cb);
					return;
				}
			}
			reinterpret_cast<void(*)(Connection*)>(cb)(nullptr);
		}));
		return 0;
	}

	int accept(SocketFD fd, Callback cb)
	{
		if (database == nullptr)
			return -1;
		if (!valid_fd(fd))
			return -1;
		auto& listener = sockets[fd];
		if (listener.state != SocketState::LISTENING)
			return -1;
		listener.portRef->child("incoming")
			->on("child_added", cheerp::Callback([fd, cb](client::FirebaseSnapshot* snapshot)
		{
			auto& listener = sockets[fd];
			auto* incomingRef = snapshot->get_ref();
			SocketFD newFd = socket();
			SocketData& news = sockets[newFd];
			auto* obj = snapshot->val<client::Object>();
			auto* offer = static_cast<client::RTCSessionDescriptionInit*>((*obj)["offer"]);
			Port clientPort = double(*(*obj)["port"]);
			auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
			news.state = SocketState::CONNECTING;
			news.portRef = listener.portRef;
			news.port = listener.port;
			news.conn = new client::RTCPeerConnection(iceConf);
			news.conn->setRemoteDescription(offer);
			Address clientAddr = idx_to_addr(addrToKeys.size());
			addrToKeys.pushBack(new client::String("client")); // TODO use some real key
			Connection* newConn = new Connection{newFd, clientAddr, clientPort, news.port};
			for (int i = 0; i < candidates->get_length(); ++i)
			{
				news.conn->addIceCandidate((*candidates)[i]);
			}
			news.conn->createAnswer()
				->then(cheerp::Callback([newConn, incomingRef, cb](client::RTCSessionDescriptionInit* answer)
			{
				SocketData& news = sockets[newConn->fd];
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				news.conn->setLocalDescription(answer);

				news.conn->set_onicecandidate(cheerp::Callback([newConn, answer, candidates, incomingRef, cb](client::RTCPeerConnectionIceEvent* e)
				{
					SocketData& news = sockets[newConn->fd];
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
					incomingRef->child("accept")->update(CHEERP_OBJECT(answer, candidates));

					news.conn->addEventListener("datachannel", cheerp::Callback([newConn, cb](client::ChannelEvent* e)
					{
						SocketData& news = sockets[newConn->fd];
						news.channel = e->get_channel();
						news.channel->set_binaryType("arraybuffer");
						news.channel->addEventListener("message", cheerp::Callback([newConn](client::MessageEvent* e)
						{
							SocketData& news = sockets[newConn->fd];
							client::ArrayBuffer* data = (client::ArrayBuffer*)e->get_data();
							news.readQueue.pushBack(new client::Uint8Array(data));
							if (news.recvCb != nullptr)
								reinterpret_cast<void(*)(Connection*)>(news.recvCb)(newConn);
						}));
						news.state = SocketState::READY;
						// TODO addr and port
						reinterpret_cast<void(*)(Connection*)>(cb)(newConn);
					}));

				}));
			}));
		}));
		return 0;
	}
	int send(SocketFD fd, uint8_t* buf, int len)
	{
		if (!ready_fd(fd))
			return -1;
		client::Uint8Array* tbuf = cheerp::MakeTypedArray(buf, len);
		sockets[fd].channel->send(*tbuf);
		return len;
	}
	int recv(SocketFD fd, uint8_t* buf, int len)
	{
		if (!ready_fd(fd))
			return -1;

		SocketData& s = sockets[fd];
		if (s.readQueue.size() == 0)
			return 0;
		client::Uint8Array& recvBuf = *s.readQueue[0];
		int toRead = len <= recvBuf.get_length() ? len : recvBuf.get_length();
		for(int i=0;i<toRead;i++)
			buf[i] = recvBuf[i];
		s.readQueue.erase(&s.readQueue[0]);

		return toRead;
	}
	int recv_callback(SocketFD fd, Callback cb)
	{
		if (!ready_fd(fd))
			return -1;
		sockets[fd].recvCb = cb;
		if (sockets[fd].readQueue.size() != 0)
			reinterpret_cast<void(*)(SocketFD)>(sockets[fd].recvCb)(fd);
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
		SocketData& s = sockets[fd];
		if (s.state == SocketState::LISTENING)
			s.portRef->remove();
		s.state = SocketState::CLOSED;
		return -1;
	}
	Address resolve(client::String* key)
	{
		for (int i = 0; i < addrToKeys.size(); ++i)
		{
			if (addrToKeys[i] == key)
				return idx_to_addr(i);
		}
		addrToKeys.pushBack(key);
		return idx_to_addr(addrToKeys.size()-1);
	}
}

extern "C"
{

[[cheerp::genericjs]]
int cheerpNetListen(int fd, int port)
{
	return cheerpnet::listen(fd, port);
}
[[cheerp::genericjs]]
int cheerpNetAccept(int fd, CheerpNetCallback cb)
{
	return cheerpnet::accept(fd, cheerp::Callback(cb));
}
[[cheerp::genericjs]]
int cheerpNetConnect(CheerpNetConnection* conn, CheerpNetCallback cb)
{
	return cheerpnet::connect(conn, cheerp::Callback(cb));
}
[[cheerp::genericjs]]
int cheerpNetSend(int fd, uint8_t* buf, int len)
{
	return cheerpnet::send(fd, buf, len);
}
[[cheerp::genericjs]]
int cheerpNetRecv(int fd, uint8_t* buf, int len)
{
	return cheerpnet::recv(fd, buf, len);
}
[[cheerp::genericjs]]
int cheerpNetRecvCallback(int fd, CheerpNetCallback cb)
{
	return cheerpnet::recv_callback(fd, cheerp::Callback(cb));
}
[[cheerp::genericjs]]
int cheerpNetSocket()
{
	return cheerpnet::socket();
}
[[cheerp::genericjs]]
int cheerpNetClose(int fd)
{
	return cheerpnet::close(fd);
}

}