# Cheerpnet

A virtual LAN and socket library for the Web, based on WebRTC

## Build and install

```
mkdir build
cd build
cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=/opt/cheerp/share/cmake/Modules/CheerpToolchain.cmake  -DCMAKE_INSTALL_PREFIX=/opt/cheerp
ninja && sudo ninja install
```

## Use

See `cheerpnet.h` for the API and the `example` folder for a minimal application.

You need API keys for a Firebase Realtime Database, and populate a FirebaseConfig
object with the required info, then pass it to the init function.


