cmake_minimum_required(VERSION 3.19)
project(native_addon_hooker)

set(CMAKE_CXX_STANDARD 14)

add_executable(native_addon_hooker hooker.cc)
include_directories(node_modules/node-addon-api)
