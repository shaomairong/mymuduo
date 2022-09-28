#!/bin/bash
#chmod +x autobuild.sh 给脚本执行权限
#执行这个脚本要使用root权限：sudo ./autobuild.sh
set -e

# 如果没有build目录，创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*
#把build文件夹下面所有文件都删除
cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..

# 把头文件拷贝到 /usr/include/mymuduo  so库拷贝到 /usr/lib    PATH
#包含头文件的时候加上/mymuduo/头文件
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in `ls *.h`  #把当前目录下面的头文件拷贝到系统头文件mymuduo下面
do
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib  #拷贝so库

ldconfig  #给环境变量加了新的so库后，执行ldconfig刷新一下动态库缓存
