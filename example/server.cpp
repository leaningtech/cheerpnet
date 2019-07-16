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
	cheerpnet::accept(l, cheerp::Callback([l](cheerpnet::SocketFD fd, cheerpnet::Address a, cheerpnet::Port p)
	{
		client::console.log("connected");
		int ret = cheerpnet::recv_callback(fd, cheerp::Callback([l](cheerpnet::SocketFD fd) {
			uint8_t buf[256];
			int len = cheerpnet::recv(fd, buf, 256);
			client::console.log("received : ",(char*)buf);
			cheerpnet::recv_callback(fd, nullptr);
			cheerpnet::send(fd, buf, len);
			cheerpnet::close(fd);
			cheerpnet::close(l);
			client::console.log("end");
		}));
		assert(ret == 0);
	}));
	client::console.log("exit main");
	return 0;
}