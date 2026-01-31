#ifndef QX_DRIVER_HPP
#define QX_DRIVER_HPP

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <stdlib.h>

// =============================================================================
// 驱动接口定义
// =============================================================================

enum OPERATIONS {
    OP_INIT_KEY = 0x800,      // 初始化通信密钥
    OP_READ_MEM = 0x801,      // 内存读取操作
    OP_WRITE_MEM = 0x802,     // 内存写入操作
    OP_MODULE_BASE = 0x803,   // 获取模块基址
};

// 内存传输结构体 - 用于ioctl内存读写
typedef struct _COPY_MEMORY {
    pid_t pid;                // 目标进程ID
    uintptr_t addr;           // 目标地址
    void* buffer;             // 用户空间缓冲区
    size_t size;              // 操作大小
} COPY_MEMORY, *PCOPY_MEMORY;

// 模块信息结构体 - 用于获取so/exe基址
typedef struct _MODULE_BASE {
    pid_t pid;                // 目标进程ID
    char name[256];           // 模块名称
    uintptr_t base;           // 返回的基址
} MODULE_BASE, *PMODULE_BASE;

// =============================================================================
// 核心驱动类
// 封装内核驱动通信，支持多种驱动版本自动检测
// =============================================================================
class c_driver {
private:
    int fd = -1;              // 驱动文件描述符
    pid_t pid = 0;            // 当前绑定进程ID

    // -------------------------------------------------------------------------
    // 系统工具方法
    // -------------------------------------------------------------------------
    
    // 执行shell命令并返回输出，自动处理LD_PRELOAD防止递归
    std::string execCom(const std::string& shell) {
        std::string result;
        int pipefd[2];
        if (pipe(pipefd) == -1) return "";

        pid_t child_pid = fork();
        if (child_pid == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            return "";
        }

        if (child_pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            unsetenv("LD_PRELOAD");
            execl("/system/bin/sh", "sh", "-c", shell.c_str(), (char*)NULL);
            _exit(EXIT_FAILURE);
        } else {
            close(pipefd[1]);
            char buffer[256];
            ssize_t bytesRead;
            while ((bytesRead = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
                result.append(buffer, bytesRead);
            }
            close(pipefd[0]);
            waitpid(child_pid, NULL, 0);
        }
        return result;
    }

    // 创建设备节点，绕过system()调用
    bool createDriverNode(const std::string& path, int major, int minor) {
        dev_t dev = makedev(major, minor);
        return (mknod(path.c_str(), S_IFCHR | 0666, dev) == 0);
    }

    void removeDeviceNode(const std::string& path) {
        unlink(path.c_str());
    }

    // 从sysfs读取设备主设备号
    int getMEN(const std::string& path) {
        FILE* file = fopen(path.c_str(), "r");
        if (!file) return 0;
        int major = 0, minor = 0;
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "%d:%d", &major, &minor) == 2 && minor == 0) {
                fclose(file);
                return major;
            }
        }
        fclose(file);
        return 0;
    }

    bool hasDigit(const std::string& str) {
        return std::any_of(str.begin(), str.end(), ::isdigit);
    }

    // -------------------------------------------------------------------------
    // 驱动检测策略 - QXv5 (已知设备名扫描)
    // 扫描/dev下特定名称的字符设备
    // -------------------------------------------------------------------------
    bool connect_qxv5() {
        const char* dev_path = "/dev";
        DIR* dir = opendir(dev_path);
        if (!dir) return false;

        const std::vector<std::string> known_files = { 
            "wanbai", "CheckMe", "Ckanri", "lanran", "video188" 
        };
        struct dirent* entry;

        while ((entry = readdir(dir)) != nullptr) {
            if (std::string(entry->d_name) == "." || 
                std::string(entry->d_name) == "..") continue;

            std::string full_path = std::string(dev_path) + "/" + entry->d_name;

            // 策略1: 匹配已知文件名
            for (const auto& name : known_files) {
                if (name == entry->d_name) {
                    closedir(dir);
                    fd = open(full_path.c_str(), O_RDWR);
                    if (fd > 0) {
                        printf("[+] QXv5 Found (Known Name): %s\n", full_path.c_str());
                        return true;
                    }
                }
            }

            // 策略2: 启发式扫描（设备特征匹配）
            struct stat file_info;
            if (stat(full_path.c_str(), &file_info) < 0) continue;

            // 过滤规则
            if (strstr(entry->d_name, "gpiochip") != nullptr) continue;
            if (hasDigit(entry->d_name) || 
                strchr(entry->d_name, '_') || 
                strchr(entry->d_name, '-') || 
                strchr(entry->d_name, ':')) continue;
            if (entry->d_name == std::string("stdin") || 
                entry->d_name == std::string("stdout") || 
                entry->d_name == std::string("stderr")) continue;

            if (!(S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode))) continue;
            if (file_info.st_atime != file_info.st_ctime) continue;

            // 特征: 空文件、root权限、短文件名
            if ((file_info.st_mode & S_IFMT) == S_IFREG && 
                file_info.st_size == 0 &&
                file_info.st_gid == 0 && 
                file_info.st_uid == 0 &&
                strlen(entry->d_name) <= 7) {
                
                closedir(dir);
                fd = open(full_path.c_str(), O_RDWR);
                if (fd > 0) {
                    printf("[+] QXv5 Found (Heuristic): %s\n", full_path.c_str());
                    return true;
                }
            }
        }
        closedir(dir);
        return false;
    }

    // -------------------------------------------------------------------------
    // 驱动检测策略 - QXv8 (sysfs虚拟设备扫描)
    // 扫描/sys/devices/virtual/下的动态创建设备
    // -------------------------------------------------------------------------
    int connect_qxv8() {
        DIR* dir = opendir("/sys/devices/virtual/");
        if (!dir) return -1;

        struct dirent* main_entry;
        while ((main_entry = readdir(dir)) != nullptr) {
            std::string main_path = "/sys/devices/virtual/" + 
                                   std::string(main_entry->d_name) + "/";
            DIR* sub_dir = opendir(main_path.c_str());
            if (!sub_dir) continue;

            struct dirent* sub_entry;
            while ((sub_entry = readdir(sub_dir)) != nullptr) {
                if (hasDigit(sub_entry->d_name) || sub_entry->d_type != DT_DIR) continue;
                std::string name = sub_entry->d_name;
                
                // 过滤: 6位长度，无特殊字符
                if (name.length() != 6 || 
                    name.find(".") != std::string::npos || 
                    name.find("_") != std::string::npos || 
                    name.find("-") != std::string::npos || 
                    name.find(":") != std::string::npos) continue;
                
                if (name == main_entry->d_name) continue;

                std::string dev_path = "/sys/class/" + 
                                      std::string(main_entry->d_name) + 
                                      "/" + name + "/dev";
                int major = getMEN(dev_path);
                if (major != 0) {
                    std::string device_node = "/dev/" + name;
                    
                    if (createDriverNode(device_node, major, 0)) {
                        int tmp_fd = open(device_node.c_str(), O_RDWR);
                        printf("[+] QXv8 Candidate: %s (fd: %d)\n", 
                               device_node.c_str(), tmp_fd);
                        
                        removeDeviceNode(device_node);

                        if (tmp_fd != -1) {
                            closedir(sub_dir);
                            closedir(dir);
                            return tmp_fd;
                        }
                    }
                }
            }
            closedir(sub_dir);
        }
        closedir(dir);
        return -1;
    }

    // -------------------------------------------------------------------------
    // 驱动检测策略 - QXv9 (进程注入残留检测)
    // 通过扫描已删除的/data/xxxxxx文件找到驱动守护进程
    // -------------------------------------------------------------------------
    bool connect_qxv9() {
        const std::string cmd = "ls -l /proc/*/exe 2>/dev/null | grep -E \"/data/[a-z]{6} \\(deleted\\)\"";
        std::string output = execCom(cmd);
        if (output.empty()) return false;

        size_t proc_pos = output.find("/proc/");
        if (proc_pos == std::string::npos) return false;
        
        size_t pid_end = output.find("/", proc_pos + 6);
        std::string pid_str = output.substr(proc_pos + 6, pid_end - (proc_pos + 6));

        size_t arrow_pos = output.find("->");
        size_t paren_pos = output.find("(");
        if (arrow_pos == std::string::npos || paren_pos == std::string::npos) return false;

        std::string file_path = output.substr(arrow_pos + 3, paren_pos - arrow_pos - 4);
        if (!file_path.empty() && file_path.front() == '\'') file_path.erase(0, 1);

        size_t data_pos = file_path.find("/data/");
        if (data_pos != std::string::npos) {
            file_path.replace(data_pos, 5, "/dev/");
        } else {
            return false;
        }

        char get_fd_cmd[256];
        sprintf(get_fd_cmd, "ls -al -L /proc/%s/fd/3", pid_str.c_str());
        std::string fd_info = execCom(get_fd_cmd);
        
        int major = 0, minor = 0;
        sscanf(fd_info.c_str(), "%*s %*d %*s %*s %d, %d", &major, &minor);

        if (major > 0) {
            if (createDriverNode(file_path, major, minor)) {
                sleep(1);
                fd = open(file_path.c_str(), O_RDWR);
                removeDeviceNode(file_path);
                if (fd > 0) {
                    printf("[+] QXv9 Connected: %s\n", file_path.c_str());
                    return true;
                }
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // 自动连接协调器 - 按优先级尝试所有检测策略
    // -------------------------------------------------------------------------
    bool auto_connect() {
        printf("[*] Starting QX Driver Auto-Detection...\n");

        fd = connect_qxv8();
        if (fd > 0) {
            printf("[+] Connected to QXv8 Driver successfully.\n");
            return true;
        }
        printf("[-] QXv8 not found.\n");

        if (connect_qxv5()) {
            printf("[+] Connected to QXv5 Driver successfully.\n");
            return true;
        }
        printf("[-] QXv5 not found.\n");

        if (connect_qxv9()) {
            printf("[+] Connected to QXv9 Driver successfully.\n");
            return true;
        }
        printf("[-] QXv9 not found.\n");

        printf("[X] All QX Driver connection attempts failed.\n");
        return false;
    }

public:
    // 构造函数 - 自动初始化驱动连接
    c_driver() {
        printf("[*] Initializing driver...\n");
        if (!auto_connect()) {
            printf("[X] Driver initialization failed. Exiting program.\n");
            exit(EXIT_FAILURE);
        }
    }

    ~c_driver() {
        if (fd > 0) close(fd);
    }

    // 绑定目标进程
    void initialize(pid_t target_pid) {
        this->pid = target_pid;
    }

    // 驱动密钥验证（如需要）
    bool initKey(const char* key) {
        return (ioctl(fd, OP_INIT_KEY, key) == 0);
    }

    // -------------------------------------------------------------------------
    // 底层内存操作接口
    // -------------------------------------------------------------------------
    bool readMem(uintptr_t addr, void* buffer, size_t size) {
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, OP_READ_MEM, &cm) == 0);
    }

    bool writeMem(uintptr_t addr, void* buffer, size_t size) {
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, OP_WRITE_MEM, &cm) == 0);
    }

    // -------------------------------------------------------------------------
    // 模板化类型安全接口
    // -------------------------------------------------------------------------
    template <typename T>
    T read(uintptr_t addr) {
        T res{};
        readMem(addr, &res, sizeof(T));
        return res;
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return writeMem(addr, &value, sizeof(T));
    }

    // 备用写入方法 - 直接操作/proc/pid/mem（无需驱动）
    template <typename T>
    bool WriteAddress(uintptr_t addr, T value) {
        char lj[128];
        sprintf(lj, "/proc/%d/mem", pid);
        int handle = open(lj, O_RDWR | O_SYNC);
        if (handle < 0) return false;
        pwrite64(handle, &value, sizeof(T), addr);
        close(handle);
        return true;
    }

    // 获取模块加载基址
    uintptr_t getModuleBase(const char* name) {
        MODULE_BASE mb;
        mb.pid = this->pid;
        strncpy(mb.name, name, sizeof(mb.name) - 1);
        mb.name[sizeof(mb.name) - 1] = '\0';

        if (ioctl(fd, OP_MODULE_BASE, &mb) != 0) {
            return 0;
        }
        return mb.base;
    }
};

// =============================================================================
// C风格全局接口层
// 为纯C代码或外部接口提供兼容层
// =============================================================================

static c_driver *driver = nullptr;

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
    driver = new (std::nothrow) c_driver();

    if (driver == nullptr) {
        printf("[!] driver 分配失败\n");
        exit(EXIT_FAILURE);
    }

    printf("[-] driver 初始化成功\n");

    atexit(release_global_driver);
}



typedef char PACKAGENAME;
pid_t pid;

// 获取系统启动时间（毫秒）
uint64_t GetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / (1000 * 1000));
}

// 获取当前可执行文件所在目录
char *getDirectory() {
    static char buf[128];
    int rslt = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (rslt < 0 || (rslt >= sizeof(buf) - 1)) {
        return NULL;
    }
    buf[rslt] = '\0';
    for (int i = rslt; i >= 0; i--) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            break;
        }
    }
    return buf;
}

// 通过包名获取进程ID并初始化驱动
int getPID(char* PackageName) {
    FILE* fp;
    char cmd[0x100] = "pidof ";
    strcat(cmd, PackageName);
    fp = popen(cmd, "r");
    fscanf(fp, "%d", &pid);
    pclose(fp);
    if (pid > 0 && driver != nullptr) {
        driver->initialize(pid);
    }
    return pid;
}

// 验证PID有效性
bool PidExamIne() {
    char path[128];
    sprintf(path, "/proc/%d", pid);
    if (access(path, F_OK) != 0) {
        printf("\033[31;1m");
        puts("获取进程PID失败!");
        exit(1);
    }
    return true;
}

// 获取模块基址（C接口）
long getModuleBase(char* module_name) {
    uintptr_t base = 0;
    if (driver != nullptr) {
        base = driver->getModuleBase(module_name);
    }
    return base;
}

// 读取内存值（自动处理32/64位）
long ReadValue(long addr) {
    long he = 0;
    if (driver != nullptr) {
        if (addr < 0xFFFFFFFF) {
            he = driver->read<uint32_t>(addr);
        } else {
            he = driver->read<uint64_t>(addr);
            he &= 0xFFFFFFFFFFFF;
        }
    }
    return he;
}

// 读取32位整数
long ReadDword(long addr) {
    long he = 0;
    if (driver != nullptr) {
        he = driver->read<uint32_t>(addr);
    }
    return he;
}

// 读取浮点数
float ReadFloat(long addr) {
    float he = 0;
    if (driver != nullptr) {
        he = driver->read<float>(addr);
    }
    return he;
}

// 读取12字节数组（通常为Vector3）
int *ReadArray(long addr) {
    int *he = (int *)malloc(12);
    if (driver != nullptr) {
        driver->readMem(addr, he, 12);
    }
    return he;
}

// 写入32位整数
int WriteDword(long int addr, int value) {
    if (driver != nullptr) {
        driver->write<uint32_t>(addr, value);
    }
    return 0;
}

// 写入浮点数
int WriteFloat(long int addr, float value) {
    if (driver != nullptr) {
        driver->write<float>(addr, value);
    }
    return 0;
}



#endif // QX_DRIVER_HPP
