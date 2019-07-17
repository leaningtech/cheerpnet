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
	cheerpnet::Connection* conn = new cheerpnet::Connection {s, srv, 555, 0};
	int ret = cheerpnet::connect(conn, cheerp::Callback([](cheerpnet::Connection* conn) {
		client::console.log("connected to ", conn->remote_addr, conn->remote_port);
		cheerpnet::recv_callback(conn->fd, cheerp::Callback([](cheerpnet::Connection* conn) {
			uint8_t buf[256];
			int len = cheerpnet::recv(conn->fd, buf, 256);
			client::console.log("received back: ",(char*)buf);
			cheerpnet::recv_callback(conn->fd, nullptr);
			cheerpnet::close(conn->fd);
			client::console.log("end");
		}));
		const char msg[] = "hello webrtc!";
		int len = sizeof(msg);
		client::console.log("sending: ", msg);
		cheerpnet::send(conn->fd, (uint8_t*)msg, len);
	}));
}