# Gg_Docking_Kernel

- 使gg修改器对接内核驱动，实现内核读取内存，过大部分的搜索检测

## 各种可对接的驱动
1.KPM-KMA-RW-DRIVER：内核管理器可用的内核模块(硬件读取)
2.driverGT1_567RTdev：GT驱动和RT驱动这两者的dev方案(兼容性高)
3.driverGT2_12RThook：GT驱动和RT驱动这两者的hook方案(隐秘性高)
4.driverDitNetlink_Prohook：吃坤挂哥巴巴托斯连理枝自用驱动

## 编译完成品使用方法步骤

- 进入`Compiled_finished_product`文件夹后找到你想要用的对应驱动名称文件夹

> 把文件夹内全部内容移动并替换掉gg修改器私有目录下
> \
(比如：
> \
`/data/user/0/com.apocalua.run/files/AppHidden-85uS/`
> \
这个目录下)
> \
每个gg修改器包名不同，目录文件夹名称也不同，勿钻牛角尖。



## 如何自行编译这个项目

- 首先准备好android-ndk环境

> 第一步：\
> 把项目对应驱动文件夹源码下载解压到一个空文件目录下\
把`build.sh`文件中的`/data/user/0/com.termux/files/home/android-ndk/ndk/29.0.14206865/ndk-build`替换成你的ndk-build安装所在目录

> 第二步：\
`cd`+文件夹路径 进入该目录


> 第三步：\
执行sh文件开始编译
```sh
./build.sh
```


> 最后一步：\
等待提示项目编译完成就会在build.sh所在文件目录/libs/arm64-v8a/下生成编译完成的`libKernelGg.so`文件了


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
> 我演示的是用的96版本的so内存库，如果你想要改101版本的gg修改器，那么就需要在本项目的101version文件夹下找到lib5.so.primary后替换掉我演示的96版本的lib5.so.primary文件。
> 96版本的lib5.so.primary文件大小是3.9mb左右
> 101版本的lib5.so.primary文件大小是4.4mb左右
