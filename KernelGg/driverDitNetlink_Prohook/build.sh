#! /bin/bash

# 指定ndk路径
NDK=/data/user/0/com.termux/files/home/android-ndk/ndk/29.0.14206865/ndk-build

# 设置当前环境路径
path=$(pwd)
cd $path

# 清除缓存
if [[ -d $path/obj ]]
then
    cd $path/obj
    rm -rf $path/obj/local
    echo "清除缓存"
    cd $path
fi

# 开始编译
if [[ -f $NDK ]]
then
    $NDK clean
    $NDK V=1
else
    echo "NDK不存在/异常"
fi

# 复制文件
if [[ -f $path/libs/arm64-v8a/libKernelGg.so ]]
then
    cp -fp $path/libzy/* $path/libs/arm64-v8a/
    echo -e "$(tput setaf 2)libKernelGg.so编译成功\n存放目录：$path/libs/arm64-v8a/路径下$(tput sgr0)\n"
else
    echo "编译失败"
fi
