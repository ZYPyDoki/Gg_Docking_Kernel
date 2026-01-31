#ifndef PARADISE_DRIVER_USER_H
#define PARADISE_DRIVER_USER_H

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
#define PARADISE_INSTALL_MAGIC2 0xCAFEBABE

// Driver name in /proc/self/fd
#define PARADISE_DRIVER_NAME "[paradise_driver]"

// IOCTL command structures
struct paradise_get_pid_cmd
{
    pid_t pid;
    char name[256];
};

struct paradise_get_module_base_cmd
{
    pid_t pid;
    char name[256];
    uintptr_t base;
    int vm_flag;
};

struct paradise_memory_cmd
{
    pid_t pid;
    uintptr_t src_va;
    uintptr_t dst_va;
    size_t size;
    uintptr_t phy_addr;
};

struct paradise_memory_ioremap_cmd
{
    pid_t pid;
    uintptr_t src_va;
    uintptr_t dst_va;
    size_t size;
    uintptr_t phy_addr;
    int prot;
};

struct paradise_gyro_config_cmd
{
    int enable;
    uint32_t type_mask;
    float x;
    float y;
};

#define PARADISE_GYRO_MASK_GYRO (1u << 0)
#define PARADISE_GYRO_MASK_UNCAL (1u << 1)
#define PARADISE_GYRO_MASK_ALL (PARADISE_GYRO_MASK_GYRO | PARADISE_GYRO_MASK_UNCAL)

enum HW_BREAKPOINT_ACTION
{
    HW_BP_ADD = 1,
    HW_BP_REMOVE = 2,
};

enum HW_BREAKPOINT_TYPE
{
    HW_BP_TYPE_EXECUTE = 0,
    HW_BP_TYPE_WRITE = 1,
    HW_BP_TYPE_RW = 2,
};

struct paradise_hw_breakpoint_ctl_cmd
{
    pid_t pid;
    uintptr_t addr;
    int action;
    int type;
    int len;
};

struct user_pt_regs
{
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

struct paradise_hw_breakpoint_hit_info
{
    pid_t pid;
    uint64_t timestamp;
    uintptr_t addr;
    struct user_pt_regs regs;
};

struct paradise_hw_breakpoint_get_hits_cmd
{
    uintptr_t buffer;
    size_t count;
};

struct paradise_touch_down_cmd
{
    int slot;
    int x;
    int y;
};

struct paradise_touch_move_cmd
{
    int slot;
    int x;
    int y;
};

struct paradise_touch_up_cmd
{
    int slot;
};

// IOCTL commands
#define PARADISE_IOCTL_GET_PID _IOWR('W', 11, struct paradise_get_pid_cmd)                               // 查找进程
#define PARADISE_IOCTL_GET_MODULE_BASE _IOWR('W', 10, struct paradise_get_module_base_cmd)               // 获取模块基地址
#define PARADISE_IOCTL_READ_MEMORY _IOWR('W', 9, struct paradise_memory_cmd)                             // 硬件读
#define PARADISE_IOCTL_WRITE_MEMORY _IOWR('W', 12, struct paradise_memory_cmd)                           // 硬件写
#define PARADISE_IOCTL_READ_MEMORY_IOREMAP _IOWR('W', 16, struct paradise_memory_ioremap_cmd)            // 内核映射读
#define PARADISE_IOCTL_WRITE_MEMORY_IOREMAP _IOWR('W', 17, struct paradise_memory_ioremap_cmd)           // 内核映射写
#define PARADISE_IOCTL_GYRO_CONFIG _IOWR('W', 21, struct paradise_gyro_config_cmd)                       // 陀螺仪
#define PARADISE_IOCTL_TOUCH_DOWN _IOWR('W', 22, struct paradise_touch_down_cmd)                         // 触摸按下
#define PARADISE_IOCTL_TOUCH_MOVE _IOWR('W', 23, struct paradise_touch_move_cmd)                         // 触摸移动
#define PARADISE_IOCTL_TOUCH_UP _IOWR('W', 24, struct paradise_touch_up_cmd)                             // 触摸抬起
#define PARADISE_IOCTL_HW_BREAKPOINT_CTL _IOWR('W', 25, struct paradise_hw_breakpoint_ctl_cmd)           // 硬件断点控制
#define PARADISE_IOCTL_HW_BREAKPOINT_GET_HITS _IOWR('W', 26, struct paradise_hw_breakpoint_get_hits_cmd) // 获取硬件断点命中信息

struct TouchState
{
    uint32_t device_min_x, device_max_x;
    uint32_t device_min_y, device_max_y;
    uint32_t screen_width, screen_height;
    int screen_orientation; // 0=正常, 1=90度, 2=180度, 3=270度
    float device_scale_x, device_scale_y;
    bool initialized;

    TouchState() : device_min_x(0), device_max_x(0), device_min_y(0), device_max_y(0),
                   screen_width(0), screen_height(0), screen_orientation(0),
                   device_scale_x(0.0f), device_scale_y(0.0f), initialized(false) {}
};

static TouchState g_touch_state;

// 检查是否为触摸设备
bool is_touch_device(int fd)
{
    // 检查设备是否支持多点触控
    unsigned long abs_bits[(ABS_MAX + sizeof(long) * 8 - 1) / (sizeof(long) * 8)] = {0};

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0)
    {
        return false;
    }

    // 检查是否支持必要的多点触控事件
    auto test_bit = [](int bit, const unsigned long *array) -> bool
    {
        return (array[bit / (sizeof(long) * 8)] >> (bit % (sizeof(long) * 8))) & 1;
    };

    bool has_slot = test_bit(ABS_MT_SLOT, abs_bits);
    bool has_tracking_id = test_bit(ABS_MT_TRACKING_ID, abs_bits);
    bool has_pos_x = test_bit(ABS_MT_POSITION_X, abs_bits);
    bool has_pos_y = test_bit(ABS_MT_POSITION_Y, abs_bits);

    return has_slot && has_tracking_id && has_pos_x && has_pos_y;
}



// 坐标转换辅助函数
void convert_screen_to_physical(int screen_x, int screen_y, int *phys_x, int *phys_y)
{
    int physical_x = 0, physical_y = 0;

    switch (g_touch_state.screen_orientation)
    {
    case 0: // 正常
        physical_x = screen_x;
        physical_y = screen_y;
        break;
    case 1: // 90度
        physical_x = g_touch_state.screen_height - screen_y;
        physical_y = screen_x;
        break;
    case 3: // 270度
        physical_x = screen_y;
        physical_y = g_touch_state.screen_width - screen_x;
        break;
    default: // 180度
        physical_x = g_touch_state.screen_height - screen_y;
        physical_y = g_touch_state.screen_width - screen_x;
        break;
    }

    *phys_x = physical_x;
    *phys_y = physical_y;
}

void convert_physical_to_device(int phys_x, int phys_y, uint32_t *dev_x, uint32_t *dev_y)
{
    if (g_touch_state.device_scale_x > 0.0f && g_touch_state.device_scale_y > 0.0f)
    {
        *dev_x = (uint32_t)((float)phys_x / g_touch_state.device_scale_x);
        *dev_y = (uint32_t)((float)phys_y / g_touch_state.device_scale_y);
    }
    else
    {
        // scale 未初始化，直接使用物理坐标
        *dev_x = (uint32_t)phys_x;
        *dev_y = (uint32_t)phys_y;
    }

    // 限幅到设备范围内
    if (*dev_x < g_touch_state.device_min_x)
        *dev_x = g_touch_state.device_min_x;
    if (*dev_x > g_touch_state.device_max_x)
        *dev_x = g_touch_state.device_max_x;
    if (*dev_y < g_touch_state.device_min_y)
        *dev_y = g_touch_state.device_min_y;
    if (*dev_y > g_touch_state.device_max_y)
        *dev_y = g_touch_state.device_max_y;
}

class Paradise_hook_driver
{
private:
    pid_t pid;
    int fd;

    std::mutex driver_lock;

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
    Paradise_hook_driver()
    {
        fd = get_driver_fd();

        if (fd < 0)
        {
            printf("无法找到驱动\n");
            exit(1);
        }

        printf("识别到Paradise Driver | fd %d\n", fd);
    }

    ~Paradise_hook_driver()
    {
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }

    pid_t get_pid(const char *name)
    {
        if (fd < 0)
        {
            return false;
        }

        struct paradise_get_pid_cmd cmd = {0, ""};

        strncpy(cmd.name, name, sizeof(cmd.name) - 1);

        if (ioctl(fd, PARADISE_IOCTL_GET_PID, &cmd) != 0)
        {
            return 0;
        }

        return cmd.pid;
    }

    void initialize(pid_t target_pid)
    {
        this->pid = target_pid;
    }

    uintptr_t get_module_base(const char *name)
    {
        if (this->pid <= 0)
        {
            return 0;
        }

        struct paradise_get_module_base_cmd cmd = {this->pid, "", 0, 0};

        strncpy(cmd.name, name, sizeof(cmd.name) - 1);
        if (ioctl(fd, PARADISE_IOCTL_GET_MODULE_BASE, &cmd) != 0)
            return 0;
        return cmd.base;
    }

    bool gyro_update(float x, float y, uint32_t type_mask = PARADISE_GYRO_MASK_ALL, bool enable = true)
    {
        if (fd < 0)
        {
            return false;
        }

        struct paradise_gyro_config_cmd cmd{};

        cmd.enable = enable ? 1 : 0;
        cmd.type_mask = type_mask ? type_mask : PARADISE_GYRO_MASK_ALL;
        cmd.x = x;
        cmd.y = y;
        return ioctl(fd, PARADISE_IOCTL_GYRO_CONFIG, &cmd) == 0;
    }

    bool read(uintptr_t addr, void *buffer, size_t size)
    {
        if (fd < 0)
        {
            return false;
        }

        if (buffer == nullptr || this->pid <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(this->driver_lock);

        struct paradise_memory_ioremap_cmd cmd = {};

        cmd.pid = this->pid;
        cmd.src_va = addr;
        cmd.dst_va = (uintptr_t)buffer;
        cmd.size = size;
        cmd.prot = 0; // WMT_NORMAL

        return ioctl(fd, PARADISE_IOCTL_READ_MEMORY_IOREMAP, &cmd) == 0;
    }

    template <typename T>
    T read(uintptr_t addr)
    {
        T res{};
        if (this->read(addr, &res, sizeof(T)))
            return res;
        return {};
    }

    bool write(uintptr_t addr, void *buffer, size_t size)
    {
        if (fd < 0)
        {
            return false;
        }

        if (buffer == nullptr || this->pid <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(this->driver_lock);

        struct paradise_memory_ioremap_cmd cmd = {};

        cmd.pid = this->pid;
        cmd.src_va = addr;
        cmd.dst_va = (uintptr_t)buffer;
        cmd.size = size;
        cmd.prot = 0; // WMT_NORMAL

        return ioctl(fd, PARADISE_IOCTL_WRITE_MEMORY_IOREMAP, &cmd) == 0;
    }

    template <typename T>
    T write(uintptr_t addr, T value)
    {
        return this->write(addr, &value, sizeof(T));
    }

    bool read_safe(uintptr_t addr, void *buffer, size_t size)
    {
        if (fd < 0)
        {
            return false;
        }

        if (buffer == nullptr || this->pid <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(this->driver_lock);

        struct paradise_memory_cmd cmd = {};

        cmd.pid = this->pid;
        cmd.src_va = addr;
        cmd.dst_va = (uintptr_t)buffer;
        cmd.size = size;

        return ioctl(fd, PARADISE_IOCTL_READ_MEMORY, &cmd) == 0;
    }

    template <typename T>
    T read_safe(uintptr_t addr)
    {
        T res{};
        if (this->read_safe(addr, &res, sizeof(T)))
            return res;
        return {};
    }

    bool write_safe(uintptr_t addr, void *buffer, size_t size)
    {
        if (fd < 0)
        {
            return false;
        }

        if (buffer == nullptr || this->pid <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(this->driver_lock);

        struct paradise_memory_cmd cmd = {};

        cmd.pid = this->pid;
        cmd.src_va = addr;
        cmd.dst_va = (uintptr_t)buffer;
        cmd.size = size;

        return ioctl(fd, PARADISE_IOCTL_WRITE_MEMORY, &cmd) == 0;
    }

    template <typename T>
    T write_safe(uintptr_t addr, T value)
    {
        return this->write_safe(addr, &value, sizeof(T));
    }



    bool touch_down(int slot, int x, int y)
    {
        if (fd < 0)
        {
            return false;
        }

        uint32_t dev_x, dev_y;

        // 如果触摸已初始化，进行坐标转换
        if (g_touch_state.initialized)
        {
            int phys_x, phys_y;
            convert_screen_to_physical(x, y, &phys_x, &phys_y);
            convert_physical_to_device(phys_x, phys_y, &dev_x, &dev_y);
        }
        else
        {
            // 未初始化，直接使用传入坐标作为设备坐标
            dev_x = (uint32_t)x;
            dev_y = (uint32_t)y;
        }

        paradise_touch_down_cmd cmd = {};
        cmd.slot = slot;
        cmd.x = dev_x;
        cmd.y = dev_y;

        return ioctl(fd, PARADISE_IOCTL_TOUCH_DOWN, &cmd) == 0;
    }

    bool touch_move(int slot, int x, int y)
    {
        if (fd < 0)
        {
            return false;
        }

        uint32_t dev_x, dev_y;

        // 如果触摸已初始化，进行坐标转换
        if (g_touch_state.initialized)
        {
            int phys_x, phys_y;
            convert_screen_to_physical(x, y, &phys_x, &phys_y);
            convert_physical_to_device(phys_x, phys_y, &dev_x, &dev_y);
        }
        else
        {
            // 未初始化，直接使用传入坐标作为设备坐标
            dev_x = (uint32_t)x;
            dev_y = (uint32_t)y;
        }

        paradise_touch_move_cmd cmd = {};
        cmd.slot = slot;
        cmd.x = x;
        cmd.y = y;

        return ioctl(fd, PARADISE_IOCTL_TOUCH_MOVE, &cmd) == 0;
    }

    bool touch_up(int slot)
    {
        if (fd < 0)
        {
            return false;
        }

        paradise_touch_up_cmd cmd = {};
        cmd.slot = slot;

        return ioctl(fd, PARADISE_IOCTL_TOUCH_UP, &cmd) == 0;
    }

    bool hw_breakpoint_add(pid_t target_pid, uintptr_t addr, int type, int len)
    {
        if (fd < 0)
        {
            return false;
        }

        struct paradise_hw_breakpoint_ctl_cmd cmd = {};

        cmd.pid = target_pid;
        cmd.addr = addr;
        cmd.action = HW_BP_ADD;
        cmd.type = type;
        cmd.len = len;

        return ioctl(fd, PARADISE_IOCTL_HW_BREAKPOINT_CTL, &cmd) == 0;
    }

    bool hw_breakpoint_remove(pid_t target_pid, uintptr_t addr)
    {
        if (fd < 0)
        {
            return false;
        }

        struct paradise_hw_breakpoint_ctl_cmd cmd = {};

        cmd.pid = target_pid;
        cmd.addr = addr;
        cmd.action = HW_BP_REMOVE;
        cmd.type = 0; // no need
        cmd.len = 0;  // no need

        return ioctl(fd, PARADISE_IOCTL_HW_BREAKPOINT_CTL, &cmd) == 0;
    }

    size_t hw_breakpoint_get_hits(struct paradise_hw_breakpoint_hit_info *hit_buffer, size_t buffer_size)
    {
        if (fd < 0 || hit_buffer == nullptr || buffer_size == 0)
        {
            return 0;
        }

        struct paradise_hw_breakpoint_get_hits_cmd cmd = {};

        cmd.buffer = (uintptr_t)hit_buffer;
        cmd.count = buffer_size;

        if (ioctl(fd, PARADISE_IOCTL_HW_BREAKPOINT_GET_HITS, &cmd) == 0)
        {
            return cmd.count;
        }

        return 0;
    }
};

Paradise_hook_driver *Paradise_hook = nullptr; // 全局声明，初始化为空指针

static Paradise_hook_driver *driver = nullptr;

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
    driver = new (std::nothrow) Paradise_hook_driver();

    if (driver == nullptr) {
        printf("[!] driver 分配失败\n");
        exit(EXIT_FAILURE);
    }

    printf("[-] driver 初始化成功\n");

    atexit(release_global_driver);
}








#endif // PARADISE_DRIVER_USER_H