cmake_minimum_required(VERSION 2.5)
project(netServer)

# mymuduo最终编译成so动态库 设置动态库的路径 放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息 以及启用c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 设置项目编译头文件搜索路径 -I
include_directories(${PROJECT_SOURCE_DIR}/include)

# 继续在src 路径下找CMakeLists 
add_subdirectory(src)
