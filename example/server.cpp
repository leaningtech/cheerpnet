#include "config.h"
#include "cheerpnet.h"

[[cheerp::genericjs]]
int main()
{
	cheerpnet::init(config::firebase(), config::ice());
	cheerpnet::SocketFD l = cheerpnet::socket();
	int ret = cheerpnet::listen(l, 555);
	assert(ret==0 && "error listening");
	client::console.log("key: ", cheerpnet::local_key());
	cheerpnet::accept(l, cheerp::Callback([l](cheerpnet::Connection* conn)
	{
		client::console.log("connected to ", conn->remote_addr, conn->remote_port);
		int ret = cheerpnet::recv_callback(conn->fd, cheerp::Callback([l](cheerpnet::Connection* conn) {
			uint8_t buf[256];
			int len = cheerpnet::recv(conn->fd, buf, 256);
			client::console.log("received : ",(char*)buf);
			cheerpnet::recv_callback(conn->fd, nullptr);
			cheerpnet::send(conn->fd, buf, len);
			cheerpnet::close(conn->fd);
			client::console.log("closed");
		}));
		assert(ret == 0);
	}));
	client::window.addEventListener("beforeunload", cheerp::Callback([l](client::Event* e){
		e->preventDefault();
		cheerpnet::close(l);
		return true;
	}));

	client::console.log("exit main");

	return 0;
}