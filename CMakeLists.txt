cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")
#add_definitions(-DNO_REALPATH)

add_library(mhttpdpp microhttpdpp.cpp mhttpdfiles.cpp)
add_executable(firstbitsd firstbitsd.cpp firstbits.cpp )
target_link_libraries(mhttpdpp microhttpd)
target_link_libraries(firstbitsd mhttpdpp)
