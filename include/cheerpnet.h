#ifndef __CHEERPNET__
#define __CHEERPNET__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
#include <stdint.h>

typedef void (*CheerpNetCallback)();

__attribute__((cheerp_genericjs)) int cheerpNetBind(int fd, uint32_t addr, uint16_t port);
__attribute__((cheerp_genericjs)) int cheerpNetSendTo(int fd, uint8_t* buf, int len, uint32_t addr, uint16_t port);
__attribute__((cheerp_genericjs)) int cheerpNetRecvFrom(int fd, uint8_t* buf, int len, uint32_t* addr, uint16_t* port);
__attribute__((cheerp_genericjs)) int cheerpNetRecvCallback(CheerpNetCallback cb);
__attribute__((cheerp_genericjs)) uint32_t cheerpNetResolve(const char* name);
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
	using Port = uint16_t;
	struct [[cheerp::genericjs]] AddrInfo
	{
		Address addr;
		Port port;
	};
	// Callback signature: void(SocketFD fd);
	using Callback = client::EventListener*;
	int bind(SocketFD fd, AddrInfo* addr);
	int sendto(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr);
	int recvfrom(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr);
	int recvCallback(Callback cb);
	SocketFD socket();
	int close(SocketFD fd);
	Address resolve(client::String* key);
	client::String* reverseResolve(Address addr);
	client::String* local_key();
	void init(client::FirebaseConfig* fb, client::RTCConfiguration* ice);
}

#endif //__cplusplus

#endif //__CHEERPNET__