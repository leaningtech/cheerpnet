#ifndef __CHEERPNET__
#define __CHEERPNET__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
#include <stdint.h>
struct __attribute__((cheerp_genericjs)) CheerpNetAddrInfo
{
	uint32_t addr;
	uint32_t port;
};

typedef void (*CheerpNetCallback)(int);

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
	using AddrInfo = CheerpNetAddrInfo;
	// Callback signature: void(SocketFD fd);
	using Callback = client::EventListener*;
	int bind(SocketFD fd, AddrInfo* addr);
	int sendto(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr);
	int recvfrom(SocketFD fd, uint8_t* buf, int len, AddrInfo* addr);
	int recvCallback(Callback cb);
	SocketFD socket();
	int close(SocketFD fd);
	Address resolve(client::String*);
	client::String* local_key();
	void init(client::FirebaseConfig* fb, client::RTCConfiguration* ice);
}

#endif //__cplusplus

#endif //__CHEERPNET__