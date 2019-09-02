#include "config.h"
#include "cheerpnet.h"

[[cheerp::genericjs]]
int main()
{
	cheerpnet::init(config::firebase(), config::ice());
	client::console.log("exit main");
	return 0;
}

[[cheerp::genericjs]]
static void receive(cheerpnet::SocketFD fd)
{
		static uint8_t buf[256];
		cheerpnet::AddrInfo srv_addr;
		int len = cheerpnet::recvfrom(fd, buf, 256, &srv_addr);
		client::console.log("receiving");
		if (len <= 0)
			return;
		client::console.log("received : ",(char*)buf);
		client::console.log("from ", srv_addr.addr, ":", srv_addr.port);
		client::console.log("echoing...");
		client::setTimeout(cheerp::Callback([fd, len, srv_addr]()
		{
			cheerpnet::sendto(fd, buf, len, &srv_addr);
		}), 2000);
}
[[cheerp::genericjs]] [[cheerp::jsexport]]
extern "C" void run(client::String* key)
{
	client::console.log("run()");
	cheerpnet::AddrInfo srv_addr;
	srv_addr.addr = cheerpnet::resolve(key);
	srv_addr.port = 555;
	cheerpnet::SocketFD fd = cheerpnet::socket();
	const char msg[] = "hello webrtc!";
	int len = sizeof(msg);
	cheerpnet::recvCallback(cheerp::make_closure([fd]()
	{
		receive(fd);
	}));
	client::console.log("sending: ", msg);
	client::console.log("to ", srv_addr.addr, ":", srv_addr.port);
	cheerpnet::sendto(fd, (uint8_t*)msg, len, &srv_addr);

	static double timeout = 0;
	client::window.addEventListener("beforeunload", cheerp::Callback([](client::Event* e)
	{
		cheerpnet::suspend();
		timeout = client::setTimeout(cheerp::Callback([]()
		{
			timeout = client::setTimeout(cheerp::Callback([]()
			{
				cheerpnet::resume();
			}), 2000);
		}), 0);
		e->preventDefault();
		e->set_returnValue("");
		client::console.log("beforeunload");
		return true;
	}));
	client::window.addEventListener("unload", cheerp::Callback([](client::Event* e)
	{
		client::clearTimeout(timeout);
		client::console.log("unload");
	}));
}
