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
	cheerpnet::accept(l, [](cheerpnet::SocketFD fd, cheerpnet::Address a, cheerpnet::Port p)
	{
		client::console.log("connected");
	});
	client::console.log("exit main");
	return 0;
}