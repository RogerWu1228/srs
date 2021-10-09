cmake_minimum_required(VERSION 3.10.0)
SET(CMAKE_C_COMPILER "/usr/local/bin/gcc")
SET(CMAKE_CXX_COMPILER "/usr/local/bin/g++")
project(quick_tunnel_client)

include_directories(/usr/local/include/c++/7.5.0)
#include_directories(/usr/local/include)
aux_source_directory(. SRC_LIST)
aux_source_directory(../common COMMON)
set(PATH ../out/)

set(QUICK /root/quick/out/lib/libquick.so)

add_executable(quick_tunnel_client ${SRC_LIST} ${COMMON})
target_link_libraries(quick_tunnel_client pthread ${QUICK} dl)

