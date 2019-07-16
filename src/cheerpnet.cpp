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

	struct HandshakeData
	{
		String* get_str();
		RTCPeerConnection* get_conn();
	};
	struct DataChannel: public EventTarget
	{
		void send(String*);
		void send(Uint8Array&);
		void set_binaryType(const String&);
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
	};
	client::RTCConfiguration* iceConf = nullptr;
	utils::Vector<SocketData> sockets;
	client::FirebaseDatabase* database = nullptr;
	utils::Vector<client::String*> addrToKeys;
}
namespace [[cheerp::genericjs]] cheerpnet
{
	static bool valid_fd(SocketFD fd)
	{
		return fd >= 0 && fd < sockets.size();
	}
	static bool valid_addr(Address a)
	{
		return a >= 0 && a < addrToKeys.size() && addrToKeys[a] != nullptr;
	}
	void init(client::FirebaseConfig* fb, client::RTCConfiguration* ice)
	{
		client::firebase.initializeApp(fb);
		iceConf = ice;
		database = client::firebase.database();
		addrToKeys.pushBack(nullptr);
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
	static void connect_handshake(SocketFD fd, ConnectCallback cb)
	{
		SocketData& s = sockets[fd];
		s.state = SocketState::CONNECTING;
		s.conn = new client::RTCPeerConnection(iceConf);
		s.channel = getDataChannel(s.conn);
		s.channel->set_binaryType("arraybuffer");
		s.conn->createOffer()->then(cheerp::Callback([&s, cb, fd](client::RTCSessionDescriptionInit* offer){
			auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
			s.conn->setLocalDescription(offer);
			s.conn->set_onicecandidate(cheerp::Callback([&s, offer, candidates, cb, fd](client::RTCPeerConnectionIceEvent* e){
				if (e->get_candidate() != nullptr)
				{
					candidates->push(e->get_candidate()->toJSON());
					return;
				}
				auto* incomingRef = s.portRef->child("incoming")->push();
				client::String* accept = new client::String();
				incomingRef->set(CHEERP_OBJECT(offer, candidates, accept));
				incomingRef
					->child("accept")
					->on("value", cheerp::Callback([&s, incomingRef, cb, fd](client::FirebaseSnapshot* snapshot)
				{
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
					cb(fd);
				}));
			}));
		}));
	}
	int connect(SocketFD fd, Address srv_addr, Port srv_port, ConnectCallback cb)
	{
		if (database == nullptr)
			return -1;
		if (!valid_fd(fd))
			return -1;
		if (!valid_addr(srv_addr))
			return -1;
		client::String* peerKey = addrToKeys[srv_addr];
		database->ref("/peers")->child(peerKey)
			->once("value")
			->then(cheerp::Callback([fd, cb, srv_port, peerKey](client::FirebaseSnapshot* snapshot)
		{
			auto* obj = snapshot->val<client::Object>();
			auto* keys = (client::TArray<client::String>*)client::Object::keys(obj);
			for (int i = 0; i < keys->get_length(); ++i)
			{
				client::String* key = (*keys)[i];
				auto* value = (client::FirebasePortData*)(*obj)[*key];
				if (value->get_port() == srv_port)
				{
					sockets[fd].peerKey = peerKey;
					sockets[fd].portRef = database->ref("/peers")->child(peerKey)->child(key);
					connect_handshake(fd, cb);
					return;
				}
			}
			cb(-1);
		}));
		return 0;
	}

	int accept(SocketFD fd, AcceptCallback cb)
	{
		auto& listener = sockets[fd];
		listener.portRef->child("incoming")
			->on("child_added", cheerp::Callback([&listener, cb, fd](client::FirebaseSnapshot* snapshot)
		{
			auto* incomingRef = snapshot->get_ref();
			SocketFD newFd = socket();
			SocketData& news = sockets[newFd];
			auto* obj = snapshot->val<client::Object>();
			auto* offer = static_cast<client::RTCSessionDescriptionInit*>((*obj)["offer"]);
			auto* candidates = static_cast<client::TArray<client::RTCIceCandidateInit>*>((*obj)["candidates"]);
			news.state = SocketState::CONNECTING;
			news.portRef = listener.portRef;
			news.conn = new client::RTCPeerConnection(iceConf);
			news.conn->setRemoteDescription(offer);
			for (int i = 0; i < candidates->get_length(); ++i)
			{
				news.conn->addIceCandidate((*candidates)[i]);
			}
			news.conn->createAnswer()
				->then(cheerp::Callback([&news, incomingRef, fd, cb](client::RTCSessionDescriptionInit* answer)
			{
				auto* candidates = new client::TArray<client::RTCIceCandidateInit>();
				news.conn->setLocalDescription(answer);

				news.conn->set_onicecandidate(cheerp::Callback([&news, answer, candidates, incomingRef, cb, fd](client::RTCPeerConnectionIceEvent* e){
					if (e->get_candidate() != nullptr)
					{
						candidates->push(e->get_candidate()->toJSON());
						return;
					}
					incomingRef->child("accept")->update(CHEERP_OBJECT(answer, candidates));

					news.conn->addEventListener("datachannel", cheerp::Callback([&news, cb, fd](client::ChannelEvent* e)
					{
						news.channel = e->get_channel();
						news.channel->set_binaryType("arraybuffer");
						news.channel->addEventListener("message", cheerp::Callback([](client::MessageEvent* e)
						{
							//TODO
						}));
						news.state = SocketState::READY;
						cb(fd, 1, 1); // TODO addr and port
					}));

				}));
			}));
		}));
		return 0;
	}
	int write(SocketFD fd, uint8_t* buf, int len)
	{
		return -1;
	}
	int read(SocketFD fd, uint8_t* buf, int len)
	{
		return -1;
	}
	SocketFD socket()
	{
		SocketFD fd = sockets.size();
		sockets.pushBack(SocketData());
		return fd;
	}
	int close(SocketFD fd)
	{
		return -1;
	}
	Address resolve(client::String* key)
	{
		for (int i = 0; i < addrToKeys.size(); ++i)
		{
			if (addrToKeys[i] == key)
				return i;
		}
		addrToKeys.pushBack(key);
		return addrToKeys.size()-1;
	}
}