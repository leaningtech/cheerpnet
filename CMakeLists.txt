CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
# Rename as appropiate
PROJECT (cheerpnet)

FILE(GLOB LIBSOURCES
	./src/*.cpp
)
FILE(GLOB CLIENTSOURCES
	./example/client.cpp
	./example/config.cpp
)
FILE(GLOB SERVERSOURCES
	./example/server.cpp
	./example/config.cpp
)
INCLUDE_DIRECTORIES(
	./include
)

ADD_LIBRARY(cheerpnet STATIC ${LIBSOURCES})

LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

ADD_EXECUTABLE(client ${CLIENTSOURCES})
ADD_EXECUTABLE(server ${SERVERSOURCES})

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -cheerp-linear-heap-size=128 -cheerp-avoid-wasm-traps -cheerp-pretty-code")

TARGET_LINK_LIBRARIES(client cheerpnet)
TARGET_LINK_LIBRARIES(server cheerpnet)

file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/client.html
	INPUT ${CMAKE_CURRENT_SOURCE_DIR}/client.html)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/server.html
	INPUT ${CMAKE_CURRENT_SOURCE_DIR}/server.html)