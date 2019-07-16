#include "config.h"
#include "cheerpnet.h"

[[cheerp::genericjs]]
int main()
{
	cheerpnet::init(config::firebase(), config::ice());
	client::console.log("exit main");
	return 0;
}

[[cheerp::genericjs]] [[cheerp::jsexport]]
extern "C" void run(client::String* key)
{
	client::console.log("run()");
	cheerpnet::Address srv = cheerpnet::resolve(key);
	cheerpnet::SocketFD s = cheerpnet::socket();
	int ret = cheerpnet::connect(s, srv, 555, cheerp::Callback([](cheerpnet::SocketFD s) {
		client::console.log("connected!");
		cheerpnet::recv_callback(s, cheerp::Callback([](cheerpnet::SocketFD s) {
			uint8_t buf[256];
			int len = cheerpnet::recv(s, buf, 256);
			client::console.log("received back: ",(char*)buf);
			cheerpnet::recv_callback(s, nullptr);
			cheerpnet::close(s);
			client::console.log("end");
		}));
		const char msg[] = "hello webrtc!";
		int len = sizeof(msg);
		client::console.log("sending: ", msg);
		cheerpnet::send(s, (uint8_t*)msg, len);
	}));
}