#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <pthread.h>
#include <cstdarg>
#include <android/log.h>
#include <unistd.h> // for sysconf


#if !defined(__aarch64__)
#error "This library is optimized and restricted to AArch64 (64-bit) only."
#endif

#include "ditPro_kpm.h"

#define TAG "ggNativeHook"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOG_ONCE(msg) do{ static bool f=false; if(!f){f=true; LOGI(msg);} }while(0)


#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

using syscall_func_t = long (*)(long, ...);
static syscall_func_t g_orig_syscall = nullptr;
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_last_pid = -1;

// AArch64 syscall内联汇编实现，避免PLT跳转开销
__attribute__((always_inline))
static inline long fallback_syscall_impl(long number,
        long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
    register long x8 asm("x8") = number;
    register long x0 asm("x0") = arg1;
    register long x1 asm("x1") = arg2;
    register long x2 asm("x2") = arg3;
    register long x3 asm("x3") = arg4;
    register long x4 asm("x4") = arg5;
    register long x5 asm("x5") = arg6;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory"
    );
    return x0;
}

// 构造函数，库加载时自动获取原syscall符号
__attribute__((constructor))
static void init_original_syscall() {
    g_orig_syscall = reinterpret_cast<syscall_func_t>(dlsym(RTLD_NEXT, "syscall"));
    if (!g_orig_syscall) {
        LOGE("Hook failed: dlsym syscall not found");
    } else {
        LOGI("Hook success: syscall intercepted");
    }

}

// 合并连续内存iov块算法->减少驱动上下文切换次数
__attribute__((visibility("default"), noinline))
long syscall(long number, ...) {
    // hot path: 非目标调用直接透传
    if (likely(number != __NR_process_vm_readv)) {
        va_list ap;
        va_start(ap, number);
        long a1 = va_arg(ap, long), a2 = va_arg(ap, long), a3 = va_arg(ap, long);
        long a4 = va_arg(ap, long), a5 = va_arg(ap, long), a6 = va_arg(ap, long);
        va_end(ap);

        if (likely(g_orig_syscall)) {
            return g_orig_syscall(number, a1, a2, a3, a4, a5, a6);
        }
        return fallback_syscall_impl(number, a1, a2, a3, a4, a5, a6);
    }

    LOG_ONCE("Hook working: captured process_vm_readv");

    va_list ap;
    va_start(ap, number);

    // 类型安全：先统一按long提取匹配系统调用寄存器宽度，再静态转换为目标类型
    long v1 = va_arg(ap, long);
    long v2 = va_arg(ap, long);
    long v3 = va_arg(ap, long);
    long v4 = va_arg(ap, long);
    long v5 = va_arg(ap, long);
    long v6 = va_arg(ap, long);

    int pid = static_cast<int>(v1);
    const iovec* local_iov = reinterpret_cast<const iovec*>(v2);
    unsigned long liovcnt  = static_cast<unsigned long>(v3);
    const iovec* remote_iov = reinterpret_cast<const iovec*>(v4);
    unsigned long riovcnt  = static_cast<unsigned long>(v5);
    unsigned long flags   = static_cast<unsigned long>(v6);
    (void)flags;
    va_end(ap);

    // 标准 process_vm_readv 要求 liovcnt == riovcnt
    if (unlikely(!local_iov || !remote_iov || liovcnt == 0 || liovcnt != riovcnt)) {
        errno = EINVAL;
        return -1;
    }





    // 双重检查锁定模式，确保PID切换时的线程安全初始化
    if (unlikely(pid != g_last_pid)) {
        pthread_mutex_lock(&g_init_lock);
        if (pid != g_last_pid) {
            driver.pid = pid;
            g_last_pid = pid;
            LOGI("Driver attached to pid %d", pid);
        }
        pthread_mutex_unlock(&g_init_lock);
    }

    // 获取页面大小，移出循环避免重复调用 sysconf
    const size_t page_size = sysconf(_SC_PAGESIZE);
    long total_read = 0;
    uintptr_t merge_remote_addr = 0;
    uint8_t*  merge_local_buf   = nullptr;
    size_t    merge_len         = 0;
    bool      has_pending       = false;

    // 遍历iov数组，合并物理连续的内存块以减少驱动调用次数
    for (unsigned long i = 0; i < liovcnt; ++i) {
        const size_t len = local_iov[i].iov_len;
        if (unlikely(len == 0)) continue;

        uint8_t* local_buf = reinterpret_cast<uint8_t*>(local_iov[i].iov_base);
        const uintptr_t remote_addr = reinterpret_cast<uintptr_t>(remote_iov[i].iov_base);

        // 检查是否与待处理块物理连续：远程地址和本地地址均连续
        // 大小检查 (merge_len + len) <= page_size
        // 确保单次调用驱动的体积不超过 PAGE_SIZE，防止驱动因缓冲区限制而失败
        if (has_pending &&
                remote_addr == (merge_remote_addr + merge_len) &&
                local_buf   == (merge_local_buf + merge_len) &&
                (merge_len + len) <= page_size) {
            merge_len += len;
            continue;
        }

        // 提交已合并的块并统计
        if (likely(has_pending)) {
            driver.read(merge_remote_addr, merge_local_buf, merge_len);
            total_read += static_cast<long>(merge_len);
        }

        // 开启新合并窗口
        merge_remote_addr = remote_addr;
        merge_local_buf   = local_buf;
        merge_len         = len;
        has_pending       = true;
    }

    // 处理最后一个待提交的合并块
    if (likely(has_pending)) {
        driver.read(merge_remote_addr, merge_local_buf, merge_len);
        total_read += static_cast<long>(merge_len);
    }

    return total_read;
}