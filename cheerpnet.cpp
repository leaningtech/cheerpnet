#include "cheerpnet.h"
#include <vector>
#include <cheerp/client.h>
#include <cheerp/clientlib.h>
#include <cmath>

namespace [[cheerp::genericjs]] client
{
	struct FirebaseConfig
	{
		void set_apiKey(const String&);
		void set_authDomain(const String&);
		void set_databaseURL(const String&);
		void set_projectId(const String&);
		void set_storageBucket(const String&);
		void set_messagingSenderId(const String&);
	};
	struct Firebase
	{
		void initializeApp(FirebaseConfig*);
		FirebaseDatabase* database();
	};
	extern Firebase& firebase;
	FirebaseDatabase* database;
	struct HandshakeData
	{
		String* get_str();
		RTCPeerConnection* get_conn();
	};
	Promise* setupHandshakeReceiver(EventListener*);
	Promise* startHandshakeRequest(const String&);
	void endHandshakeRequest(String* s);

	struct ChannelEvent: public Event
	{
		DataChannel* get_channel();
	};
	struct ConnectionData
	{
		DataChannel* get_channel();
	};
	extern TArray<ConnectionData>* connections;
};

struct [[cheerp::genericjs]] CheerpSocket
{
	int port;
	caUtils::caVector<client::Uint8Array*> packets;
	// Temporary queue to store packets while the connection is established
	caUtils::caVector<client::Uint8Array*> outPackets;
};

[[cheerp::genericjs]] caUtils::caVector<CheerpSocket> sockets;
std::function<void()> netCallback;
std::function<void()> readyCallback;
[[cheerp::genericjs]] client::String* strOffer;

bool serverMode = false;

[[cheerp::genericjs]][[cheerp::jsexport]]
extern "C" void cheerpNetSetOffer(client::String* s)
{
	strOffer = s;
}

[[cheerp::genericjs]][[cheerp::jsexport]]
extern "C" void cheerpNetHandleMessage(client::ArrayBuffer* buf, int origAddr)
{
	client::Uint8Array& data = *(new client::Uint8Array(buf));
	data[0] = origAddr;
	int8_t addr = data[1];
	uint16_t port = data[4] | (data[5] << 8);
	for(int i=0;i<sockets.size();i++)
	{
		CheerpSocket& s = sockets[i];
		if(s.port == port)
		{
			s.packets.pushBack(&data);
			if(netCallback)
				netCallback();
			return;
		}
	}
	client::console.log("BAD PORT", port);
}

[[cheerp::genericjs]] void cheerpNetInitClient();
[[cheerp::genericjs]] void cheerpNetInitServer();

[[cheerp::genericjs]] void cheerpNetInit(int isServer)
{
	startFirebase();
	if(isServer)
	{
		// server mode
		serverMode = true;

		client::setupHandshakeReceiver(cheerp::Callback([](client::MessageEvent* e, int index){
			cheerpNetHandleMessage((client::ArrayBuffer*)e->get_data(), index + 2);
		}))->then(cheerp::Callback([](client::EventListener* e) {
			cheerpNetSetOffer((client::String*)e);
		}));
		cheerpNetInitServer();
	}
	else
	{
		cheerpNetInitClient();
	}
}

[[cheerp::genericjs]] int cheerpNetCreateSocket(int port)
{
	int ret = sockets.size();
	//if(port == 0) __asm__("debugger");
	sockets.pushBack(CheerpSocket{port});
	return ret+1;
}

[[cheerp::genericjs]] caUtils::caVector<ConnectionData> addrToConn;
[[cheerp::genericjs]] client::TMap<client::String*, int>* knownAddresses = new client::TMap<client::String*, int>();

[[cheerp::genericjs]] int cheerpNetResolve(const char* serverId)
{
	client::String* s = new client::String(serverId);
	if (knownAddresses->has(s))
	{
		return knownAddresses->get(s);
	}
	return -1;
}
[[cheerp::genericjs]] client::String* cheerpNetReverseResolve(int addr)
{
	if (addr >= 0 && addr < addrToConn.size())
	{
		return addrToConn[addr].serverId;
	}
	return nullptr;
}

static double deg2rad(double deg)
{
	return deg*M_PI/180;
}
static double geoDistance(double lat1, double lon1, double lat2, double lon2)
{
	constexpr int R = 6371;
	double phi1 = deg2rad(lat1);
	double phi2 = deg2rad(lat2);
	double dphi = deg2rad(lat2-lat1);
	double dlam = deg2rad(lon2-lon1);
	double a = sin(dphi/2) * sin(dphi/2) + cos(phi1) * cos(phi2) * sin(dlam/2) * sin(dlam/2);
	double c = 2 * atan2(sqrt(a), sqrt(1-a));
	return R * c;
}
static int getLatency(double dist)
{

	double D1 = dist;
	constexpr double D2 = 200;
	constexpr double DA = 20;
	constexpr double EL = 15;
	constexpr double SL = 200;

	double t = (100 + DA)/100;
	t = t*D1 + D2;
	t = t/SL; 
	t = t + EL;
	return 200*t/100;
}
struct [[cheerp::genericjs]] ImportScript {
	ImportScript(const client::String& src)
	{
		client::HTMLScriptElement* script = (client::HTMLScriptElement*)client::document.createElement("script");
		script->set_async(true);
		script->set_defer(true);
		script->set_src(src);
		client::document.get_body()->appendChild(script);
	}
};
[[cheerp::genericjs]]
ImportScript geoScript("https://extreme-ip-lookup.com/json/?callback=geoIpCallback");
[[cheerp::genericjs]]
client::GeoLocation* geoLoc = nullptr;
[[cheerp::genericjs]]
[[cheerp::jsexport]]
extern "C" void geoIpCallback(client::GeoLocation* data)
{
	bool undef;
	asm("%1===undefined" : "=r"(undef) : "r"((client::Object*)data));
	if (!undef)
		geoLoc = data;
}

namespace [[cheerp::genericjs]] client
{
	struct ServerRow: public Object
	{
		void set_key(const String&);
		void set_name(const String&);
		void set_map(const String&);
		void set_type(const String&);
		void set_players(const String&);
		void set_latency(int);
		void set_continent(const String&);
	};
	void addServerRow(ServerRow* row);
	void removeServerRow(const String& key);
	void moveServerRow(const String& key, String* prevKey);
	void updateServerRow(ServerRow* row);
}

[[cheerp::genericjs]] double timeSkew = 0;

[[cheerp::genericjs]] static client::ServerRow* createServerRow(client::FirebaseSnapshot* snapshot)
{
	auto* key = snapshot->get_key();
	auto* server = snapshot->val<client::FirebaseServerData>();
	if(!server)
	{
		return nullptr;
	}
	double currentTime = client::Date::now() + timeSkew;
	if(server->get_timeStamp() < currentTime - 60000)
		return nullptr;
	auto* row = new client::ServerRow();
	row->set_key(*key);
	row->set_name(*server->get_name());
	row->set_map(*server->get_mapName());
	row->set_type(*server->get_gameType());
	row->set_players(*(new client::String(server->get_clientCount()))->concat("/")->concat(server->get_clientMax()));
	if (server->get_geo() != nullptr)
	{
		row->set_continent(*server->get_geo()->get_continent());
		if (geoLoc)
		{
			double lat1 = client::parseFloat(geoLoc->get_lat());
			double lon1 = client::parseFloat(geoLoc->get_lon());
			double lat2 = client::parseFloat(server->get_geo()->get_lat());
			double lon2 = client::parseFloat(server->get_geo()->get_lon());
			double dist = geoDistance(lat1, lon1, lat2, lon2);
			int latency = getLatency(dist);
			row->set_latency(latency);
		}
		else
			row->set_latency(-1);
	}
	return row;
}
[[cheerp::genericjs]] void cheerpNetInitServer()
{
	// Correct local time skew using firebase as reference
	client::database->ref("/.info/serverTimeOffset")
		->on("value", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
	{
		client::Number* skew = snapshot->val<client::Number>();
		asm("%1" : "=r"(timeSkew) : "r"(skew));

		auto cleanup = cheerp::Callback([](){
			// Cleanup old servers
			client::database->ref("/servers")
				->orderByChild("timeStamp")
				->endAt(client::Date::now()-30000+timeSkew)
				->once("value")
				->then(cheerp::Callback([](client::FirebaseSnapshot* snapshot)
			{
				snapshot->forEach(cheerp::Callback([](client::FirebaseSnapshot* child)
				{
					child->get_ref()->remove();
				}));
			}));
		});
		// Schedule an delayed update
		client::setTimeout(cleanup, 0);
		// Schedule an update every 5 seconds
		client::setInterval(cleanup, /*5 secs*/5000);

		auto* ref = client::database
			->ref("/servers")
			->orderByChild("timeStamp")
			->startAt(client::Date::now()-30000+timeSkew);
		ref->on("child_added", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			auto* row = createServerRow(snapshot);
			if (row == nullptr)
				return;
			client::addServerRow(row);
		}));
		ref->on("child_removed", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			client::removeServerRow(snapshot->get_key());
		}));
		ref->on("child_changed", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			auto* row = createServerRow(snapshot);
			if (row == nullptr)
				return;
			client::updateServerRow(row);
		}));
	}));
}

[[cheerp::genericjs]] static int getFirstFreeAddr()
{
	int i = 0;
	for(; i < addrToConn.size(); ++i)
	{
		if (addrToConn[i].server == nullptr)
		{
			return i;
		}
	}
	addrToConn.pushBack(ConnectionData{});
	return i;
}
[[cheerp::genericjs]] void cheerpNetInitClient()
{
	// Correct local time skew using firebase as reference
	client::database->ref("/.info/serverTimeOffset")
		->on("value", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
	{
		client::Number* skew = snapshot->val<client::Number>();
		asm("%1" : "=r"(timeSkew) : "r"(skew));

		auto* ref = client::database
			->ref("/servers")
			->orderByChild("timeStamp")
			->startAt(client::Date::now()-30000+timeSkew);
		auto setServerAddr = cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			auto* key = snapshot->get_key();
			auto* server = snapshot->val<client::FirebaseServerData>();
			int addr = 0;
			if(knownAddresses->has(key))
				addr = knownAddresses->get(key);
			else
			{
				addr = getFirstFreeAddr();
				knownAddresses->set(key, addr);
				addrToConn[addr].serverId = key;
			}
			addrToConn[addr].server = server;
		});
		ref->on("child_added", setServerAddr);
		ref->on("child_changed", setServerAddr);
		ref->on("child_removed", cheerp::Callback([](client::FirebaseSnapshot* snapshot)
		{
			auto* key = snapshot->get_key();
			knownAddresses->delete_(key);
			for(int i = 0; i < addrToConn.size(); ++i)
			{
				if (addrToConn[i].serverId == key)
				{
					addrToConn[i].serverId = nullptr;
					addrToConn[i].server = nullptr;
				}
			}
		}));
		ref->once("value")
			->then(cheerp::Callback([setServerAddr](client::FirebaseSnapshot* snapshot)
		{
			snapshot->forEach(setServerAddr);
			readyCallback();
		}));
	}));
}

[[cheerp::genericjs]] void cheerpNetSendTo(int socket, int addr, int port, const unsigned char* data, unsigned size)
{
	if(addr < 0)
		return;
	CheerpSocket& s = sockets[socket-1];
	int tmpOffset = __builtin_cheerp_pointer_offset(data);
	client::Uint8Array* tmp = __builtin_cheerp_pointer_base<client::Uint8Array>(data)->subarray(tmpOffset, tmpOffset+size);
	caUtilsAssert(tmp->get_BYTES_PER_ELEMENT() == 1);
	client::Uint8Array& n = *(new client::Uint8Array(size+6));
	// 0 is src addr, and it's set by the receiving side
	// 1 is dst addr
	n[1] = addr & 0xff;
	// 2-3 is the src port
	n[2] = s.port & 0xff;
	n[3] = (s.port >> 8) & 0xff;
	// 4-5 is the dst port
	n[4] = port & 0xff;
	n[5] = (port >> 8) & 0xff;
	n.set(tmp, 6);
	if(serverMode)
	{
		auto* conn = (*client::connections)[(addr&0xff)-2];
		if (conn)
			conn->get_channel()->send(n);
		return;
	}
	if(addr >= addrToConn.size())
		return;
	if(addrToConn[addr].channel)
		addrToConn[addr].channel->send(n);
	else
	{
		if(!addrToConn[addr].isConnecting)
		{
			caUtilsAssert(addrToConn[addr].server->get_offer());
			addrToConn[addr].isConnecting = true;
			startHandshakeRequest(addrToConn[addr].server->get_offer())->then(cheerp::Callback([addr](client::HandshakeData* data)
			{
				client::String* strAnswer = data->get_str();
				client::database->ref("/servers")->child(addrToConn[addr].serverId)->child("answer")->set(strAnswer);
				client::RTCPeerConnection* conn = data->get_conn();
				addrToConn[addr].conn = conn;
				conn->addEventListener("datachannel", cheerp::Callback([addr](client::ChannelEvent* e)
				{
					client::DataChannel* channel = e->get_channel();
					channel->set_binaryType("arraybuffer");
					addrToConn[addr].channel = channel;
					channel->addEventListener("message", cheerp::Callback([addr](client::MessageEvent* e)
					{
						client::ArrayBuffer* data = (client::ArrayBuffer*)e->get_data();
						if(client::isNaN(data->get_byteLength()))
							__asm__("debugger");
						cheerpNetHandleMessage(data, addr);
					}));
					//       Get address as assigned by server
					//       Flush pending packets
				}));
			}));
		}
	}
}

[[cheerp::genericjs]] int cheerpNetRecv(int socket, int* addr, int* port, unsigned char* data, unsigned maxsize)
{
	CheerpSocket& s = sockets[socket-1];
	if(s.packets.isEmpty())
		return 0;
	// Packets here are already filtered by addr
	client::Uint8Array& buf = *s.packets[0];
	caUtilsAssert(buf.get_length() < maxsize);
	*addr = buf[0];
	*port = buf[2] | (buf[3]<<8);
	for(int i=6;i<buf.get_length();i++)
		data[i-6] = buf[i];
	s.packets.erase(&s.packets[0]);
	return buf.get_length()-6;
}

void cheerpNetSetCallback(std::function<void()>&& callback)
{
	netCallback = std::move(callback);
}
void cheerpNetSetReadyCallback(std::function<void()>&& callback)
{
	readyCallback = std::move(callback);
}

[[cheerp::genericjs]] void startFirebase()
{
	client::FirebaseConfig* config = new client::FirebaseConfig();
	config->set_apiKey("AIzaSyCCjNnfSkuQqr047dIa6asAcjr0kqn-5as");
	config->set_authDomain("teeworlds-207014.firebaseapp.com");
	config->set_databaseURL("https://teeworlds-207014.firebaseio.com");
	config->set_projectId("teeworlds-207014");
	config->set_storageBucket("teeworlds-207014.appspot.com");
	config->set_messagingSenderId("260296572079");
	client::firebase.initializeApp(config);
	client::database = client::firebase.database();
}

[[cheerp::genericjs]] void cheerpNetEndHandshake(client::String* offer)
{
	client::endHandshakeRequest(offer);
	client::setupHandshakeReceiver(cheerp::Callback([](client::MessageEvent* e, int index){
		cheerpNetHandleMessage((client::ArrayBuffer*)e->get_data(), index + 2);
	}))->then(cheerp::Callback([](client::EventListener* e) {
		cheerpNetSetOffer((client::String*)e);
	}));
}