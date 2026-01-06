#include <errno.h> // 用于 errno 变量
#include <string.h> // 用于 strerror 函数
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <cstdarg>
#include <elf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>



#include "ditPro_kpm.h"


//日志
#include <android/log.h>

#define TAG "NativeHook"

// 定义Android Log输出宏，方便调试和日志记录
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// --- 调试开关 ---
// 集中控制调试日志，发布版本时设为0，编译器会自动优化掉所有DLOG调用，减少性能开销和包体积
#define ENABLE_DEBUG_LOGS 0

#if ENABLE_DEBUG_LOGS
    #define DLOG(...) LOGD(__VA_ARGS__)
#else
    #define DLOG(...) // 定义为空，编译器会优化掉所有调用
#endif

// --- 内存页对齐宏 ---
// 用于获取一个地址所在内存页的起始地址。内存操作通常需要按页对齐。
#define PAGE_START(addr) ((addr) & (~(sysconf(_SC_PAGESIZE) - 1)))
// 获取系统内存页的大小
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

// 全局指针，用于保存原始syscall函数的地址，以便在Hook函数中调用原始实现
static long (*original_syscall_ptr)(long, ...) = nullptr;



/**
 * @brief 优化后的自定义内存读取函数
 * @details 移除了对PAGE_SIZE的依赖，因为驱动可能支持任意大小的读取。
 *          将driver->initialize()调用移出循环，以提高多次读取时的效率。
 *          函数现在返回实际读取的字节数，以更好地模拟原始系统调用。
 * 
 * @param pid 目标进程ID
 * @param local_iov 本地iovec数组指针
 * @param liovcnt 本地iovec数量
 * @param remote_iov 远程iovec数组指针
 * @param riovcnt 远程iovec数量
 * @return long 成功读取的字节总数，失败返回-1
 */
static long vm_readv_custom(int pid, const iovec* local_iov, unsigned long liovcnt, const iovec* remote_iov, unsigned long riovcnt) {
    if (!local_iov || !remote_iov || liovcnt == 0 || liovcnt != riovcnt) {
        LOGE("vm_readv_custom: Invalid parameters");
        return -1;
    }

    // 初始化驱动传入保持最新目标pid
    driver.pid = pid;

    long total_read = 0;
    for (unsigned long i = 0; i < liovcnt; ++i) {
        // 检查iov_len是否为0，避免无意义的驱动调用
        if (local_iov[i].iov_len == 0) {
            continue;
        }

        // 调用驱动进行单块读取
        // 注意：driver->read的返回值需要被检查，这里假设它总是成功
        driver.read(
            (uintptr_t)remote_iov[i].iov_base,
            local_iov[i].iov_base,
            local_iov[i].iov_len
        );
        total_read += local_iov[i].iov_len;
    }
    return total_read;
}

/**
 * @brief 优化后的Hook函数
 * @details 当任何代码调用syscall时，会首先进入此函数。
 *          我们检查syscall号，如果是目标号（如process_vm_readv），则执行自定义逻辑，
 *          否则转发给原始的syscall函数。
 *          此版本使用`va_list`直接传递给原始函数，避免了手动解析6个参数的硬编码方式。
 *          这种写法更通用、更安全，能正确处理所有参数。
 */
long my_syscall(long number, ...) {
    // 只处理我们关心的系统调用
    if (number == __NR_process_vm_readv) {
        va_list args;
        va_start(args, number);
        // 直接从va_list中提取参数，更安全可靠
        int pid = va_arg(args, int);
        const iovec* local_iov = va_arg(args, const iovec*);
        unsigned long liovcnt = va_arg(args, unsigned long);
        const iovec* remote_iov = va_arg(args, const iovec*);
        unsigned long riovcnt = va_arg(args, unsigned long);
        // 注意：process_vm_readv还有一个flags参数，我们也需要获取
        unsigned long flags = va_arg(args, unsigned long);
        va_end(args);

        DLOG("Hooked process_vm_readv: pid=%d, liovcnt=%lu, riovcnt=%lu", pid, liovcnt, riovcnt);

        // 调用我们优化后的自定义读取函数
        long bytes_read = vm_readv_custom(pid, local_iov, liovcnt, remote_iov, riovcnt);

        // 如果自定义读取失败（返回-1），则回退到原始系统调用
        if (bytes_read < 0) {
            DLOG("Custom vm_readv failed, falling back to original syscall.");
            // 重新构造va_list以传递给原始函数
            va_list args_fallback;
            va_start(args_fallback, number);
            long result = original_syscall_ptr(number, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);
            va_end(args_fallback);
            return result;
        }
        return bytes_read;
    }

    // 对于其他系统调用，直接转发给原始函数
    // 为了最大程度兼容原始逻辑并优化，我们在这里采用一个折中方案：
    // 假设其他syscall最多6个参数，这与原始代码一致。
    va_list args;
    va_start(args, number);
    long arg1 = va_arg(args, long);
    long arg2 = va_arg(args, long);
    long arg3 = va_arg(args, long);
    long arg4 = va_arg(args, long);
    long arg5 = va_arg(args, long);
    long arg6 = va_arg(args, long);
    va_end(args);
    
    return original_syscall_ptr(number, arg1, arg2, arg3, arg4, arg5, arg6);
}

/**
 * @brief 在当前进程的内存映射中查找指定模块的基地址
 * @param module_name 要查找的模块名（如 "libc.so"）
 * @return uintptr_t 找到的模块基地址，未找到则返回0
 */
static uintptr_t get_module_base_addr(const char* module_name) {
    char line[512];
    // 打开/proc/self/maps文件，它包含了当前进程所有内存映射的信息
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;

    uintptr_t base_addr = 0;
    // 逐行解析maps文件
    while (fgets(line, sizeof(line), maps)) {
        // 查找包含可执行权限（r-xp）并且路径中包含目标模块名的行
        // 使用strstr进行子串匹配，比精确匹配整个路径更灵活
        if (strstr(line, "r-xp") && strstr(line, module_name)) {
            // 解析该行的起始地址，这就是模块的加载基地址
            if (sscanf(line, "%lx", &base_addr) == 1) {
                DLOG("Found base address for '%s': %p", module_name, (void*)base_addr);
                break; // 找到后立即退出循环
            }
        }
    }
    fclose(maps);
    return base_addr;
}

/**
 * @brief 通过修改GOT（全局偏移表）来Hook指定函数
 * @details 此函数解析ELF文件的动态链接信息，找到目标函数在GOT中的条目，
 *          然后修改其内容指向我们的Hook函数。
 * 
 * @param base_addr 目标模块的基地址
 * @param func_name 要Hook的函数名
 * @param new_func 我们的自定义Hook函数地址
 * @param original_func [out] 用于存储原始函数地址的指针
 * @return bool Hook成功返回true，失败返回false
 */
bool hook_plt_got_by_addr(uintptr_t base_addr, const char* func_name, void* new_func, void** original_func) {
    if (base_addr == 0) {
        LOGE("base_addr is 0, cannot hook.");
        return false;
    }

    // 验证ELF魔数，确保我们正在解析的是一个有效的ELF文件，防止访问无效内存导致崩溃
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base_addr;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        LOGE("ELF magic number mismatch at %p. Not a valid ELF.", (void*)base_addr);
        return false;
    }

    // 获取程序头表
    Elf64_Phdr* phdr = (Elf64_Phdr*)(base_addr + ehdr->e_phoff);
    // 用于存储从动态段中解析出的关键信息
    Elf64_Addr rela_dyn_offset = 0, rela_plt_offset = 0;
    Elf64_Word rela_dyn_count = 0, rela_plt_count = 0;
    Elf64_Sym* symtab = nullptr; // 符号表
    char* strtab = nullptr;     // 字符串表

    // 遍历程序头，找到PT_DYNAMIC类型的动态段
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            Elf64_Dyn* dyn = (Elf64_Dyn*)(base_addr + phdr[i].p_vaddr);
            // 遍历动态段中的条目，提取符号表、字符串表和重定位表信息
            for (int j = 0; dyn[j].d_tag != DT_NULL; ++j) {
                switch (dyn[j].d_tag) {
                case DT_RELA:       rela_dyn_offset = dyn[j].d_un.d_ptr; break;
                case DT_RELASZ:     rela_dyn_count = dyn[j].d_un.d_val / sizeof(Elf64_Rela); break;
                case DT_JMPREL:     rela_plt_offset = dyn[j].d_un.d_ptr; break;
                case DT_PLTRELSZ:   rela_plt_count = dyn[j].d_un.d_val / sizeof(Elf64_Rela); break;
                case DT_SYMTAB:     symtab = (Elf64_Sym*)(base_addr + dyn[j].d_un.d_ptr); break;
                case DT_STRTAB:     strtab = (char*)(base_addr + dyn[j].d_un.d_ptr); break;
                }
            }
            break; // 找到动态段后即可退出循环
        }
    }

    if (!symtab || !strtab) {
        LOGE("Failed to find symtab/strtab in dynamic section.");
        return false;
    }

    // 使用lambda表达式来封装查找和Hook的逻辑，避免代码重复
    auto find_and_hook = [&](Elf64_Rela* rela_table, Elf64_Word count) -> bool {
        for (Elf64_Word i = 0; i < count; ++i) {
            // 从重定位条目中获取符号表索引
            Elf64_Sym* sym = &symtab[ELF64_R_SYM(rela_table[i].r_info)];
            // 比较符号名，找到目标函数
            if (strcmp(strtab + sym->st_name, func_name) == 0) {
                DLOG("Found symbol '%s' at rela index %d", func_name, i);
                // 计算GOT条目的虚拟地址
                void* got_entry = (void*)(base_addr + rela_table[i].r_offset);
                if (original_func) *original_func = *(void**)got_entry;

                // 修改内存页权限为可写，这是Hook的关键步骤
                uintptr_t page_addr = PAGE_START((uintptr_t)got_entry);
                if (mprotect((void*)page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0) {
                    LOGE("mprotect failed for %p: %s", (void*)page_addr, strerror(errno));
                    return false; // 优化：明确返回失败
                }

                DLOG("Replacing GOT entry %p (original: %p) with %p", got_entry, *original_func, new_func);
                // 将GOT条目中的地址替换为我们的Hook函数地址
                *(void**)got_entry = new_func;

                // 恢复内存页权限为可读可执行，这是一个良好的安全实践
                if(mprotect((void*)page_addr, PAGE_SIZE, PROT_READ | PROT_EXEC) != 0) {
                    LOGE("mprotect (restore) failed for %p: %s", (void*)page_addr, strerror(errno));
                    // 即使恢复失败，Hook也已生效，但应记录错误
                }
                return true; // Hook成功
            }
        }
        return false; // 在当前重定位表中未找到
    };

    // 优先在.JMPREL（通常用于PLT Hook）中查找
    if (rela_plt_count > 0) {
        Elf64_Rela* rela_plt = (Elf64_Rela*)(base_addr + rela_plt_offset);
        if (find_and_hook(rela_plt, rela_plt_count)) return true;
    }
    // 如果没找到，再在.RELA（通常用于直接绑定）中查找
    if (rela_dyn_count > 0) {
        Elf64_Rela* rela_dyn = (Elf64_Rela*)(base_addr + rela_dyn_offset);
        if (find_and_hook(rela_dyn, rela_dyn_count)) return true;
    }

    LOGE("Symbol '%s' not found in any relocation table", func_name);
    return false;
}

/**
 * @brief 后台线程函数，用于等待目标库加载并执行Hook
 * @param arg 线程参数（此处未使用）
 * @return void*
 */
void* hook_thread_func(void* arg) {
    const char* target_lib = "lib5.so.primary";
    DLOG("Hook thread started, waiting for '%s'...", target_lib);

    // 引入超时机制，例如等待30秒
    const int max_wait_seconds = 30;
    const int sleep_interval_ms = 500;
    int total_wait_ms = 0;

    while (true) {
        // 循环检查目标库是否已加载到内存中
        uintptr_t base_addr = get_module_base_addr(target_lib);
        if (base_addr != 0) {
            DLOG("Target library found. Attempting to hook 'syscall'...");
            // 找到库后，尝试Hook其中的'syscall'函数
            if (hook_plt_got_by_addr(base_addr, "syscall", (void*)my_syscall, (void**)&original_syscall_ptr)) {
                LOGI("Successfully hooked 'syscall' in '%s'!", target_lib);
            } else {
                LOGE("Hooking 'syscall' failed even though library was found.");
            }
            // 无论成功或失败，任务都已完成，线程退出
            break; 
        }

        // 检查是否超时
        total_wait_ms += sleep_interval_ms;
        if (total_wait_ms >= max_wait_seconds * 1000) {
            LOGE("Timeout: Could not find '%s' after %d seconds. Hook thread exiting.", target_lib, max_wait_seconds);
            break; // 超时，线程退出
        }

        // 如果库还未加载，等待500毫秒后再次检查
        usleep(sleep_interval_ms * 1000);
    }

    DLOG("Hook thread finished and is exiting.");
    return nullptr; // 线程函数返回，线程结束
}


/**
 * @brief 构造函数属性，确保此函数在库加载时自动执行
 * @details 我们利用这个特性来启动Hook线程，从而实现对目标库的Hook。
 */
__attribute__((constructor))
static void init_hook_thread() {
    pthread_t hook_thread;
    // 创建一个分离的线程来执行Hook逻辑，避免阻塞主线程
    if (pthread_create(&hook_thread, nullptr, hook_thread_func, nullptr) != 0) {
        LOGE("Failed to create hook thread!");
    } else {
        pthread_detach(hook_thread);
    }
}
