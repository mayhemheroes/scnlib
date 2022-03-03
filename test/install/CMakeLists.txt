cmake_minimum_required(VERSION 3.2.3)
project(scn-install-test CXX)

find_package(scn REQUIRED)

add_executable(scn-install-test install-test.cpp)
target_link_libraries(scn-install-test PUBLIC scn::scn)
