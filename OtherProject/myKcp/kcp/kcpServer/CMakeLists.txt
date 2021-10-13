

cmake_minimum_required(VERSION 2.8)

project(Kcp)

aux_source_directory(. DIR_SRCS)

add_executable(KcpServer ${DIR_SRCS})

