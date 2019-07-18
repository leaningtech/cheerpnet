#include "config.h"
#include "cheerpnet.h"

[[cheerp::genericjs]]
static void echo(cheerpnet::SocketFD fd)
{
	while(true)
	{
		uint8_t buf[256];
		cheerpnet::AddrInfo client_addr;
		int len = cheerpnet::recvfrom(fd, buf, 256, &client_addr);
		if (len <= 0)
			return;
		client::console.log("received : ",(char*)buf);
		client::console.log("from ", client_addr.addr, ":", client_addr.port);
		client::console.log("echoing...");
		cheerpnet::sendto(fd, buf, len, &client_addr);
	}
}

[[cheerp::genericjs]]
int main()
{
	cheerpnet::init(config::firebase(), config::ice());
	cheerpnet::SocketFD fd = cheerpnet::socket();
	cheerpnet::AddrInfo addr { 0, 555};
	cheerpnet::recvCallback(cheerp::Callback([fd]()
	{
		echo(fd);
	}));
	cheerpnet::bind(fd, &addr);
	client::console.log("key: ", cheerpnet::local_key());
	client::console.log("exit main");

	return 0;
}