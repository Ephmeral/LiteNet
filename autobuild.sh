#!/bin/bash

set -e

# 重新创建build目录
rm -rf `pwd`/build
mkdir build

# 进入build目录进行编译运行
cd `pwd`/build &&
    cmake .. &&
    make -j4

# 回到项目根目录
cd ..

# 头文件拷贝到 /usr/include/litenet
if [ ! -d /usr/include/litenet ]; then
    mkdir /usr/include/litenet
fi

for header in `ls *.h`
do 
    cp $header /usr/include/litenet
done

# so库拷贝到 /usr/lib
cp `pwd`/lib/libLiteNet.so /usr/lib

ldconfig