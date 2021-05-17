cmake_minimum_required(VERSION 3.5)
project(ipc-test)

add_subdirectory(ipclib)

add_executable(test WIN32 main.cpp)
target_link_libraries(test PRIVATE ipclib)
