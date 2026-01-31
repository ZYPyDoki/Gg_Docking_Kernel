/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FastScan KPM 驱动用户态接口 (单头文件)
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

#ifndef _KPM_DRIVER_TEAR_GAME_H
#define _KPM_DRIVER_TEAR_GAME_H

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <cstdio>
#include <sys/syscall.h>
#include <sys/types.h>

// ==================== ioctl 定义 ====================
#ifndef _IOC_NRBITS
#define _IOC_NRBITS     8
#define _IOC_TYPEBITS   8
#define _IOC_SIZEBITS   14
#define _IOC_DIRBITS    2
#define _IOC_NRSHIFT    0
#define _IOC_TYPESHIFT  (_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT  (_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT   (_IOC_SIZESHIFT+_IOC_SIZEBITS)
#define _IOC_NONE       0U
#define _IOC_WRITE      1U
#define _IOC_READ       2U
#define _IOC(dir,type,nr,size) \
    (((dir)<<_IOC_DIRSHIFT)|((type)<<_IOC_TYPESHIFT)|((nr)<<_IOC_NRSHIFT)|((size)<<_IOC_SIZESHIFT))
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
#endif

#ifndef __NR_ioctl
#define __NR_ioctl 29
#endif

struct mem_operation {
    pid_t target_pid;
    uint64_t addr;
    void *buffer;
    uint64_t size;
};

#define OP_READ_MEM  _IOWR('v', 0x9F, struct mem_operation)
#define OP_WRITE_MEM _IOWR('v', 0xA0, struct mem_operation)

// ==================== 驱动接口 ====================
namespace KPMDriver {

// 获取进程 PID
inline pid_t getPid(const char* name) {
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir))) {
            if (entry->d_type != DT_DIR) continue;
            bool is_pid = true;
            for (const char* p = entry->d_name; *p && is_pid; p++)
                if (*p < '0' || *p > '9') is_pid = false;
            if (!is_pid) continue;
            
            char path[64], cmdline[256] = {0};
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
            FILE* fp = fopen(path, "r");
            if (fp) {
                fread(cmdline, 1, sizeof(cmdline) - 1, fp);
                fclose(fp);
                size_t len = strlen(name);
                if (strncmp(cmdline, name, len) == 0 && 
                    (cmdline[len] == '\0' || cmdline[len] == ':')) {
                    pid_t pid = atoi(entry->d_name);
                    closedir(dir);
                    return pid;
                }
            }
        }
        closedir(dir);
    }
    
    // pidof 保底
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pidof %s 2>/dev/null", name);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        pid_t pid = -1;
        fscanf(fp, "%d", &pid);
        pclose(fp);
        if (pid > 0) return pid;
    }
    return -1;
}

// 获取模块基址
inline uint64_t getModuleBase(pid_t pid, const char* name, uint64_t* size = nullptr) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;
    
    char line[512];
    uint64_t base = 0, end = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, name)) {
            uint64_t s, e;
            if (sscanf(line, "%lx-%lx", &s, &e) == 2) {
                if (!base) base = s;
                end = e;
            }
        } else if (base) break;
    }
    fclose(fp);
    if (size) *size = end - base;
    return base;
}

// 内存读取
inline int readMem(pid_t pid, uint64_t addr, void* buf, size_t len) {
    struct mem_operation op = {pid, addr, buf, len};
    return syscall(__NR_ioctl, (int)time(nullptr), OP_READ_MEM, &op);
}

// 内存写入
inline int writeMem(pid_t pid, uint64_t addr, const void* buf, size_t len) {
    struct mem_operation op = {pid, addr, const_cast<void*>(buf), len};
    return syscall(__NR_ioctl, (int)time(nullptr), OP_WRITE_MEM, &op);
}

// 模板读取
template<typename T>
inline T read(pid_t pid, uint64_t addr) {
    T val{};
    readMem(pid, addr, &val, sizeof(T));
    return val;
}

// 模板写入
template<typename T>
inline bool write(pid_t pid, uint64_t addr, const T& val) {
    return writeMem(pid, addr, &val, sizeof(T)) == 0;
}

} // namespace KPMDriver

#endif
