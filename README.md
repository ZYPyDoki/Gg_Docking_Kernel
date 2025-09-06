# Gg_Docking_Kernel

- 使gg修改器对接内核驱动，实现内核读取内存，过大部分的搜索检测


## 编译完成品使用方法步骤

- 下载\
`lib5.so`\
`lib5.so.primary`\
`libKernelGg.so`\
这三个文件

> 把刚刚下载的三个文件移动并替换掉gg修改器私有目录下
> \
(比如：
> \
`/data/user/0/com.apocalua.run/files/AppHidden-85uS/`
> \
这个目录下)
> \
每个gg修改器包名不同，目录文件夹名称也不同，勿钻牛角尖。



## 如何自行编译这个项目

- 首先准备好cmake环境

> 第一步：\
> 把项目打包下载解压到一个空文件目录下\
把`CMakeLists.txt`文件中的`/data/user/0/com.termux/files/home/android-sdk/ndk/27.1.12297006`替换成你的ndk安装目录

> 第二步：\
`cd `+文件夹路径 进入该目录


> 第三步：\
cmake初始化生成构建文件
```sh
cmake .
```


> 最后一步：\
make执行构建项目，等待项目构建完成就会在该文件目录下生成`libKernelGg.so`文件了
```sh
make
```



## 关于原理

- 来源enen大牛的思路，在gg修改器加载文件读写so库`lib5.so`之前，先加载一个共享库，加载完再加载原文件读写库`lib5.so`。从而实现hook syscall系统调用号270，替换gg的文件读写so库`lib5.so`的读为内核驱动读方式。




## 正确安装ndk过程

- 视频：`https://b23.tv/ljTKIDV`
> 一键安装命令如下：
```sh
cd && tar -xvf android-ndk-r28c-aarch64-linux-android.tar.xz && rm -rf android-ndk-r28c-aarch64-linux-android.tar.xz && mkdir android-ndk && mkdir android-ndk/ndk && mv android-ndk-r28c 28.2.13676358 && mv 28.2.13676358 android-ndk/ndk/ && ln -s $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64 && ln -s $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-x86_64 && echo 'Installation Finished. Ndk has been installed successfully!'
```




## 注意事项
- 内核GT版驱动需要解压出来到根目录路径下
> ｀比如/data/local/｀并给予777执行权限root0所有者用户组才可以执行，而不是图省事在压缩包里面执行。图省事只会导致load.sh找不到.ko文件从而提示刷入失败报错。

- 替换后要注意文件权限之类的内容
> 比如777权限，比如所有者用户组要改成gg自己的用户组。目的是让gg修改器这个APP知道这是属于他的而不是属于别人的，给予ta绝对的安全感归属感。

- 96版的so和101版本的so不一样
> 我演示的是用的96版本的so内存库，如果你想要改101版本的gg修改器，那么就需要把未替换前101版本的gg修改器私有目录下原本的lib5.so改成lib5.so.primary后替换掉我演示的lib5.so.primary文件
