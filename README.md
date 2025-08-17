# Gg_Docking_Kernel

- 使gg对接内核驱动，实现内核读取内存，过大部分的搜索检测


## 编译完成品使用方法步骤

- 下载\
`lib5.so`\
`lib5.so.primary`\
`libKernelGg.so`\
这三个文件

> 把刚刚下载的三个文件移动并替换掉gg私有目录下
> \
(比如：`/data/user/0/com.apocalua.run/files/AppHidden-85uS/`这个目录下)
> \
每个gg包名不同，目录文件夹名称也不同，勿钻牛角尖。



## 如何自行编译这个项目

- 首先准备好cmake环境

> 第一步：把项目打包下载解压到一个空文件目录下\
第二步：`cd `+文件夹路径 进入该目录


> 第三步：cmake初始化生成构建文件
```sh
cmake .
```


> 最后一步：make执行构建项目，等待项目构建完成就会在该文件目录下生成`libKernelGg.so`文件了
```sh
make
```



## 关于原理

- 来源enen大牛的思路，在gg修改器加载文件读写so库`lib5.so`之前，先加载一个共享库，加载完再加载原文件读写库`lib5.so`。从而实现hook syscall系统调用号270，替换gg的文件读写so库`lib5.so`的读为内核驱动读方式。
