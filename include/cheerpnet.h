#ifndef __CHEERPNET__
#define __CHEERPNET__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
#include <stdint.h>
struct CheerpNetConnection
{
	int fd;
	uint32_t remote_addr;
	uint32_t remote_port;
	uint32_t local_port;
};

typedef void (*CheerpNetCallback)(CheerpNetConnection*);
__attribute__((cheerp_genericjs)) int cheerpNetListen(int fd, int port);
__attribute__((cheerp_genericjs)) int cheerpNetAccept(int fd, CheerpNetCallback cb);
__attribute__((cheerp_genericjs)) int cheerpNetConnect(CheerpNetConnection* conn, CheerpNetCallback cb);
__attribute__((cheerp_genericjs)) int cheerpNetSend(int fd, uint8_t* buf, int len);
__attribute__((cheerp_genericjs)) int cheerpNetRecv(int fd, uint8_t* buf, int len);
__attribute__((cheerp_genericjs)) int cheerpNetRecvCallback(int fd, CheerpNetCallback cb);
__attribute__((cheerp_genericjs)) int cheerpNetSocket();
__attribute__((cheerp_genericjs)) int cheerpNetClose(int fd);

#ifdef __cplusplus
}
#endif //__cplusplus

#ifdef __cplusplus

#include <cheerp/client.h>
#include "firebase.h"

namespace [[cheerp::genericjs]] cheerpnet
{
	using SocketFD = int;
	using Address = uint32_t;
	using Port = uint32_t;
	using Connection = CheerpNetConnection;
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

#endif //__cplusplus

#endif //__CHEERPNET__