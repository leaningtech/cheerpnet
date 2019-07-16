#ifndef __CONFIG__
#define __CONFIG__

#include "cheerpnet.h"

namespace [[cheerp::genericjs]] config
{
    client::FirebaseConfig* firebase();
    client::RTCConfiguration* ice();
}

#endif // __FIREBASECONF__