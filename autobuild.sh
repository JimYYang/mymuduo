#!/bin/bash

set -e

# 如果没有build目录 创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build/
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..

# 把头文件拷贝到 /usr/include/netServer so库拷贝到/usr/lib
if [ ! -d /usr/include/netServer ]; then
    mkdir /usr/include/netServer
fi

for header in include/*.h;
do
    cp $header /usr/include/netServer
done

cp `pwd`/lib/libnetServer.so /usr/lib

ldconfig