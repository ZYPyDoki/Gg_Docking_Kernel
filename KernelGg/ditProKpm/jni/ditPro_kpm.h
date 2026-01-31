#include <iostream>
#include <vector>
#include <initializer_list>
#include <unistd.h>
#include <stdio.h>

#include <cstring>

#include <csignal>
#include <cstdlib>


class proKpm {
private:
    struct Ditpro_uct_kpm
    {
        int pid;
        uintptr_t addr;
        void *buffer;
        size_t size;
        uint32_t mode;
    };

    struct Ditpro_uct_list
    {
        int pid;
        uintptr_t addr[10];
        size_t addr_count;
        void *buffer;
        size_t buffer_size;
        uint32_t mode;
    };

    struct Ditpro_uct_base {
        pid_t pid;
        char* name;
        uintptr_t base;
    };

    enum {
        __NR_syscall_ = 18,
        __FLAGS = 616,
        __READMEM = 0x400,
        __READMEMLIST = 0x401,
        __WRITEMEM = 0x200,
        __MODULEBASE = 0x100, //未完成
        __PROCPID = 0x50,
        __UNINSTALL = 0x10,
        __KERNELMODE = 1, //1正常缓存映射 2正常缓存只读，写不可用 3无缓存映射，直接访问硬件

        __CALLFUNC_1 = 0x900, //过触摸检测
    };

public:
    int pid = -1;
    float mode = 1.f;

    //读取整个链条
    bool read(void *buffer,size_t buffer_size,std::initializer_list<uint64_t> args) {
        struct Ditpro_uct_list ptr;
        ptr.pid = pid;
        uintptr_t *p = ptr.addr;
        ptr.buffer = buffer;
        ptr.addr_count = args.size();
        if(ptr.addr_count > 10) return false;
        for (const auto &arg : args) {
            *p = arg;
            p ++;
        }
        ptr.buffer_size = buffer_size;
        ptr.mode = (uint32_t)this->mode;
        if(syscall(__NR_syscall_, __FLAGS, &ptr, __READMEMLIST))
            return false;
        return true;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        struct Ditpro_uct_kpm ptr;
        ptr.addr = addr;
        ptr.buffer = buffer;
        ptr.pid = this->pid;
        ptr.size = size;
        ptr.mode = (uint32_t)this->mode;
        if(syscall(__NR_syscall_, __FLAGS, &ptr, __READMEM))
            return false;
        return true;
    }

    template <typename T>
    T read(uintptr_t addr)
    {
        T res;
        if (this->read(addr, &res, sizeof(T)))
            return res;
        return {};
    }

    //失败返回 -1
    int get_pid(const char *name) {
        return syscall(__NR_syscall_, __FLAGS, name, __PROCPID);
    }

    //务必调用
    void uninstall() {
        long result = syscall(__NR_syscall_, __FLAGS, 0, __UNINSTALL);
        if (result < 0) {
            // 可以在这里记录错误日志，但不能返回错误码
            fprintf(stderr, "[!] 卸载驱动失败: %ld\n", result);
        }
    }

    ~proKpm() {
        uninstall();
    }

    //在getpid或者getbase之后调用即可
    int tscape_input(const char *name) {
        return syscall(__NR_syscall_, __FLAGS, name, __CALLFUNC_1);
    }

    uintptr_t get_module_base(int pid, const char *module_name) {
        FILE *fp;
        long addr = 0;
        char *pch;
        char filename[64];
        char line[1024];
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
        fp = fopen(filename, "r");
        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, module_name)) {
                    pch = strtok(line, "-");
                    addr = strtoul(pch, NULL, 16);
                    if (addr == 0x8000) addr = 0;
                    break;
                }
            }
            fclose(fp);
        }
        return addr;
    }
};

proKpm driver;




// 信号处理函数
static void cleanup_signal_handler(int sig) {
    fprintf(stderr, "[!] 接收到信号 %d，正在清理驱动...\n", sig);
    driver.uninstall();
    _exit(sig);  // 使用_exit避免递归清理
}

// 正常退出清理函数
static void cleanup_on_exit() {
    printf("[*] 程序退出，清理驱动资源...\n");
    driver.uninstall();
}

// 程序初始化
__attribute__((constructor))
static void reg_init_exit_func() {
    // 注册信号处理器
    struct sigaction sa;
    sa.sa_handler = cleanup_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr);  // 终止信号
    sigaction(SIGABRT, &sa, nullptr);  // 异常终止
    sigaction(SIGSEGV, &sa, nullptr);  // 段错误

    // 注册正常退出清理函数
    if (atexit(cleanup_on_exit) != 0) {
        fprintf(stderr, "[!] 无法注册退出清理函数\n");
    }

    printf("[*] ditProKpm驱动管理器初始化完成\n");
}