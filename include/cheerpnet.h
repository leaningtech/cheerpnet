#ifndef __CHEERPNET__
#define __CHEERPNET__

#include <cstdint>
#include <cheerp/client.h>

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
		void set_appId(const String&);
	};
}

namespace [[cheerp::genericjs]] cheerpnet
{
	using SocketFD = int;
	using Address = int;
	using Port = int;
	struct Connection
	{
		SocketFD fd;
		Address remote_addr;
		Port remote_port;
		Port local_port;
	};
	// Listener signature: void(Connection* conn);
	using Callback = client::EventListener*;
	int listen(SocketFD fd, Port port);
	int accept(SocketFD fd, Callback cb);
	int connect(Connection* conn, Callback cb);
	int send(SocketFD fd, uint8_t* buf, int len);
	int recv(SocketFD fd, uint8_t* buf, int len);
	int recv_callback(SocketFD fd, Callback cb);
	SocketFD socket();
	int close(SocketFD fd);
	Address resolve(client::String*);
	client::String* local_key();
	void init(client::FirebaseConfig* fb, client::RTCConfiguration* ice);
}

#endif //__CHEERPNET__