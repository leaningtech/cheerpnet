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
	int ret = cheerpnet::connect(s, srv, 555, [](cheerpnet::SocketFD s) {
		client::console.log("connected!");
	});
}