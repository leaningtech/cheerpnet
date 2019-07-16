#ifdef __cplusplus
#include <functional>
extern "C" {
#endif

__attribute__((cheerp_genericjs)) void cheerpNetInit(int isServer);
__attribute__((cheerp_genericjs)) int cheerpNetCreateSocket(int port);
__attribute__((cheerp_genericjs)) void cheerpNetSendTo(int socket, int addr, int port, const unsigned char* data, unsigned size);
__attribute__((cheerp_genericjs)) int cheerpNetRecv(int socket, int* addr, int* port, unsigned char* data, unsigned maxsize);
__attribute__((cheerp_genericjs)) int cheerpNetResolve(const char* serverId);

#ifdef __cplusplus
}

#include "cautils.h"
#include <cheerp/client.h>

void cheerpNetSetCallback(std::function<void()>&& callback);
void cheerpNetSetReadyCallback(std::function<void()>&& callback);
[[cheerp::genericjs]] void startFirebase();

namespace [[cheerp::genericjs]] client
{
	struct FirebaseReference;
	struct FirebaseDatabase
	{
		FirebaseReference* ref(const client::String&);
	};
	struct FirebaseReference
	{
		void on(const String&, EventListener*);
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
	struct GeoLocation: public Object
	{
		String* get_city();
		String* get_continent();
		String* get_lat();
		String* get_lon();
	};
	extern FirebaseDatabase* database;
	struct FirebaseServerData: public Object
	{
		void set_version(const String&);
		client::String* get_version();
		void set_name(const String&);
		client::String* get_name();
		void set_hostName(const String&);
		client::String* get_hostName();
		void set_mapName(const String&);
		client::String* get_mapName();
		void set_gameType(const String&);
		client::String* get_gameType();
		void set_flags(int);
		int get_flags();
		void set_skillLevel(int);
		int get_skillLevel();
		void set_playerCount(int);
		int get_playerCount();
		void set_playerMax(int);
		int get_playerMax();
		void set_clientCount(int);
		int get_clientCount();
		void set_clientMax(int);
		int get_clientMax();
		void set_timeStamp(double);
		double get_timeStamp();
		String* get_offer();
		void set_offer(const String&);
		String* get_answer();
		void set_answer(String*);
		double get_ping();
		void set_ping(double);
		GeoLocation* get_geo();
		void set_geo(GeoLocation*);
	};
	struct FirebaseSnapshot
	{
		template<class T>
		T* val();
		String* get_key();
		void forEach(client::EventListener* e);
		FirebaseReference* get_ref();
	};
}

namespace [[cheerp::genericjs]] client
{
	struct DataChannel: public EventTarget
	{
		void send(String*);
		void send(Uint8Array&);
		void set_binaryType(const String&);
	};
}
struct [[cheerp::genericjs]] ConnectionData
{
	client::FirebaseServerData* server;
	client::String* serverId;
	client::DataChannel* channel;
	client::RTCPeerConnection* conn;
	bool isConnecting = false;
};

[[cheerp::genericjs]] extern caUtils::caVector<ConnectionData> addrToConn;

[[cheerp::genericjs]] void cheerpNetEndHandshake(client::String* offer);

[[cheerp::genericjs]] client::String* cheerpNetReverseResolve(int addr);

#endif
