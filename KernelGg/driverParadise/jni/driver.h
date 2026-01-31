#pragma once
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <stddef.h>
#include <mutex>

// define __NR_reboot if not defined
#ifndef __NR_reboot
#if defined(__aarch64__) || defined(__arm__)
#define __NR_reboot 142
#elif defined(__x86_64__)
#define __NR_reboot 169
#elif defined(__i386__)
#define __NR_reboot 88
#elif defined(SYS_reboot)
#define __NR_reboot SYS_reboot
#else
#error "__NR_reboot not defined and cannot be determined for this architecture"
#endif
#endif

// Magic numbers for reboot syscall to install fd
#define PARADISE_INSTALL_MAGIC1 0xDEADBEEF
#define PARADISE_INSTALL_MAGIC2 0xF00DCAFE

// Driver name in /proc/self/fd
#define PARADISE_DRIVER_NAME "[paradise_driver]"

struct paradise_read_physical_memory_ioremap_cmd
{
    pid_t pid;          /* Input: Process ID owning the virtual address */
    uintptr_t src_va;   /* Input: Virtual address to access */
    uintptr_t dst_va;   /* Input: Virtual address to write */
    size_t size;        /* Input: Size of memory to read */
    uintptr_t phy_addr; /* Output: Physical address of the source virtual address */
    int prot;           /* Input: Memory protection type (use MT_*) */
};

// --- IOCTL 命令 ---
#define PARADISE_IOCTL_READ_MEMORY_IOREMAP _IOWR('W', 16, struct paradise_read_physical_memory_ioremap_cmd)

#define WMT_NORMAL 0
#define WMT_NORMAL_TAGGED 1
#define WMT_NORMAL_NC 2
#define WMT_NORMAL_WT 3
#define WMT_DEVICE_nGnRnE 4
#define WMT_DEVICE_nGnRE 5
#define WMT_DEVICE_GRE 6
#define WMT_NORMAL_iNC_oWB 7

// --- 单例 Driver 类 ---

class Driver
{
private:
    int fd;
    pid_t pid; // 保存初始化后的目标进程PID

        int scan_driver_fd()
    {
        DIR *dir = opendir("/proc/self/fd");
        if (!dir)
        {
            return -1;
        }

        struct dirent *entry;
        char link_path[256];
        char target[256];
        ssize_t len;

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            int fd_num = atoi(entry->d_name);
            if (fd_num < 0)
            {
                continue;
            }

            snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd_num);
            len = readlink(link_path, target, sizeof(target) - 1);
            if (len < 0)
            {
                continue;
            }
            target[len] = '\0';

            // Check if this is our driver
            if (strstr(target, PARADISE_DRIVER_NAME) != NULL)
            {
                closedir(dir);
                return fd_num;
            }
        }

        closedir(dir);
        return -1;
    }

    int install_driver_fd()
    {
        int fd = -1;

        long ret = syscall(__NR_reboot, PARADISE_INSTALL_MAGIC1, PARADISE_INSTALL_MAGIC2, 0, &fd);

        if (fd < 0)
        {
            printf("install_driver_fd: fd not installed, ret=%ld, errno=%d\n", ret, errno);
            return -1;
        }

        printf("install_driver_fd: fd=%d installed successfully\n", fd);
        return fd;
    }

    int get_driver_fd()
    {
        int fd = scan_driver_fd();
        if (fd >= 0)
        {
            return fd;
        }

        fd = install_driver_fd();
        if (fd >= 0)
        {
            return fd;
        }

        return -1;
    }

public:

    // 构造函数
    Driver()
    {
        fd = get_driver_fd();

        if (fd < 0)
        {
            printf("无法找到驱动\n");
            exit(1);
        }

        printf("识别到Paradise Driver | fd %d\n", fd);
    }

    // 构析函数
    ~Driver()
    {
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }

    // 初始化pid，创建一个读写对象务必要初始一次，pid范围为 (0,32768) 开区间
    void initialize(pid_t target_pid) { this->pid = target_pid; }

    // 内核层读取数据，只读已映射到内核空间的地址，传入地址、接收指针、类型大小，读前记录CPU缓存状态，读完后恢复CPU缓存行状态，支持多线程，效率较高
    bool read(uintptr_t addr, void *buffer, size_t size)
    {
        if (buffer == nullptr || this->pid <= 0)
            return false;

        paradise_read_physical_memory_ioremap_cmd cmd = {};

        cmd.pid = this->pid;
        cmd.src_va = addr;
        cmd.dst_va = (uintptr_t)buffer;
        cmd.size = size;
        cmd.prot = WMT_NORMAL;

        return ioctl(fd, PARADISE_IOCTL_READ_MEMORY_IOREMAP, &cmd) == 0;
    }

    // 模板方法，传入地址，返回地址上的值，支持多线程
    template <typename T>
    T read(uintptr_t addr)
    {
        T res{};
        if (this->read(addr, &res, sizeof(T)))
            return res;
        return {};
    }
};





static Driver *driver = nullptr;

// 全局驱动清理
static void release_global_driver() {
    if (driver != nullptr) {
        delete driver;
        driver = nullptr;
        printf("[*] 全局driver已释放！\n");
    }
}

// 注册程序退出时的清理函数
__attribute__((constructor)) 
static void reg_init_func() {

    // 使用 nothrow，失败时返回 nullptr
    driver = new (std::nothrow) Driver();

    if (driver == nullptr) {
        printf("[!] driver 分配失败\n");
        exit(EXIT_FAILURE);
    }

    printf("[-] driver 初始化成功\n");

    atexit(release_global_driver);
}