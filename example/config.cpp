#include "config.h"

namespace [[cheerp::genericjs]] config
{

client::FirebaseConfig* firebase()
{
	client::FirebaseConfig* config = new client::FirebaseConfig();
	config->set_apiKey("AIzaSyBs8JacPn_RZKqTdZnDyqy2O9vlMs9gBgw");
	config->set_authDomain("cheerpnet.firebaseapp.com");
	config->set_databaseURL("https://cheerpnet.firebaseio.com");
	config->set_projectId("cheerpnet");
	config->set_storageBucket("");
	config->set_messagingSenderId("175893724825");
	config->set_appId("1:175893724825:web:6d8bfbdb9c9095f0");
	return config;
}

client::RTCConfiguration* ice()
{
	client::RTCConfiguration* conf;
	asm("{'iceServers':[{'urls':['turn:136.243.170.209:3478'],'username':'tee','credential':'tee'},{'urls':['stun:stun.l.google.com:19302'],'username':'','credential':''}],'iceTransportPolicy':'all','iceCandidatePoolSize':'0'}"
	: "=r"(conf)
	:);
	return conf;
}

}