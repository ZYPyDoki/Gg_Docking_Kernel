#include <cstdint>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define IOCTL_AMM_READ_MEM 0x7E1A
#define IOCTL_AMM_WRITE_MEM 0x7E1B
#define IOCTL_AMM_FIND_PID 0x7E1C
#define IOCTL_AMM_FIND_MODULE 0x7E1D
#define MAX_PROC_NAME_LEN 256
#define BUFFER_POOL_SIZE 4096

typedef struct {
    uint32_t pid;
    uintptr_t addr;
    void *buffer;
    size_t size;
} COPY_MEMORY;

typedef struct {
    char proc_name[MAX_PROC_NAME_LEN];
    pid_t result_pid;
} FIND_PID_REQ;

typedef struct {
    uint32_t pid;
    char module_name[MAX_PROC_NAME_LEN];
    uintptr_t result_addr;
} FIND_MODULE_REQ;

class c_driver {
  private:
    int mSocketFd;
    pid_t target_pid;
    void *mBufferPool;
    size_t mBufferPoolSize;

    c_driver(const c_driver &) = delete;
    c_driver &operator=(const c_driver &) = delete;

    // 获取指定大小的缓冲区
    void *getBuffer(size_t size) {
        if (size == 0)
            return nullptr;

        if (size <= mBufferPoolSize)
            return mBufferPool;

        void *newBuf = realloc(mBufferPool, size);
        if (!newBuf)
            return nullptr;

        mBufferPool = newBuf;
        mBufferPoolSize = size;
        return mBufferPool;
    }

  public:
    // 初始化驱动连接
    c_driver() : mSocketFd(-1), target_pid(-1), mBufferPool(nullptr), mBufferPoolSize(0) {
        mSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
        mBufferPool = malloc(BUFFER_POOL_SIZE);
        if (mBufferPool)
            mBufferPoolSize = BUFFER_POOL_SIZE;
    }

    // 清理资源
    ~c_driver() {
        if (mSocketFd >= 0) {
            close(mSocketFd);
            mSocketFd = -1;
        }
        if (mBufferPool) {
            free(mBufferPool);
            mBufferPool = nullptr;
        }
        mBufferPoolSize = 0;
    }

    // 检查连接状态
    bool isConnected() const {
        return mSocketFd >= 0;
    }

    // 设置目标进程ID
    void set_target_pid(pid_t pid) {
        if (pid > 0)
            target_pid = pid;
    }

    // 获取目标进程ID
    pid_t get_target_pid() const {
        return target_pid;
    }

    // 通过进程名获取进程ID
    pid_t get_pid(const std::string &pkgName) {
        if (mSocketFd < 0 || pkgName.empty())
            return -1;

        FIND_PID_REQ req = {};
        strncpy(req.proc_name, pkgName.c_str(), MAX_PROC_NAME_LEN - 1);
        req.proc_name[MAX_PROC_NAME_LEN - 1] = '\0';

        if (ioctl(mSocketFd, IOCTL_AMM_FIND_PID, &req) < 0)
            return -1;

        return req.result_pid;
    }

    // 获取模块基地址
    uintptr_t getModuleBase(pid_t pid, const std::string &moduleName) {
        if (mSocketFd < 0 || pid <= 0 || moduleName.empty())
            return 0;

        FIND_MODULE_REQ req = {};
        req.pid = pid;
        strncpy(req.module_name, moduleName.c_str(), MAX_PROC_NAME_LEN - 1);
        req.module_name[MAX_PROC_NAME_LEN - 1] = '\0';

        if (ioctl(mSocketFd, IOCTL_AMM_FIND_MODULE, &req) < 0)
            return 0;

        return req.result_addr;
    }

    // 读取内存
    bool read(uint64_t addr, void *buffer, size_t size) {
        if (mSocketFd < 0 || target_pid <= 0 || !buffer || size == 0)
            return false;

        void *temp_buf = getBuffer(size);
        if (!temp_buf)
            return false;

        COPY_MEMORY req = {};
        req.pid = target_pid;
        req.addr = addr;
        req.buffer = temp_buf;
        req.size = size;

        int ret = ioctl(mSocketFd, IOCTL_AMM_READ_MEM, &req);
        if (ret <= 0)
            return false;

        memcpy(buffer, temp_buf, ret);
        return true;
    }

    // 写入内存
    bool write(uint64_t addr, const void *buffer, size_t size) {
        if (mSocketFd < 0 || target_pid <= 0 || !buffer || size == 0)
            return false;

        void *temp_buf = getBuffer(size);
        if (!temp_buf)
            return false;

        memcpy(temp_buf, buffer, size);

        COPY_MEMORY req = {};
        req.pid = target_pid;
        req.addr = addr;
        req.buffer = temp_buf;
        req.size = size;

        int ret = ioctl(mSocketFd, IOCTL_AMM_WRITE_MEM, &req);
        return ret > 0;
    }

    // 读取单个值
    template <typename T>
    bool read(uint64_t addr, T *value) {
        if (!value)
            return false;
        return read(addr, value, sizeof(T));
    }

    // 写入单个值
    template <typename T>
    bool write(uint64_t addr, const T &value) {
        return write(addr, &value, sizeof(T));
    }
};


c_driver driver;

