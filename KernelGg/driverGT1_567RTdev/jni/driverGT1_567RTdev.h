#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vector>
#include <string>

class c_driver
{
private:
    int fd;
    pid_t pid;

    typedef struct _COPY_MEMORY
    {
        pid_t pid;
        uintptr_t addr;
        void *buffer;
        size_t size;
    } COPY_MEMORY, *PCOPY_MEMORY;

    typedef struct _MODULE_BASE
    {
        pid_t pid;
        char *name;
        uintptr_t base;
    } MODULE_BASE, *PMODULE_BASE;

    struct process
    {
        pid_t process_pid;
        char process_comm[15];
    };

    enum OPERATIONS
    {
        OP_INIT_KEY = 0x800,
        OP_READ_MEM = 0x801,
        OP_WRITE_MEM = 0x802,
        OP_MODULE_BASE = 0x803,
        OP_HIDE_PROCESS = 0x804,
    };

    char *driver_path()
    {
        printf("\033[32m;1m welcome to kernel.h by Cycle1337 \033[0m\n");

        const char *dev_path = "/dev";
        DIR *dir = opendir(dev_path);
        if (dir == NULL)
        {
            printf("\033[31m;1m [!] failed to open /dev \033[0m\n");
            return NULL;
        }

        const std::vector<std::string> excluded_names = {
            "binder", "common", "ashmem", "stdin", "stdout", "stderr"};

        struct dirent *entry;
        char *file_path = NULL;
        while ((entry = readdir(dir)) != NULL)
        {
            const char *current_name = entry->d_name;

            if (strcmp(current_name, ".") == 0 || strcmp(current_name, "..") == 0)
            {
                continue;
            }

            if (strstr(current_name, "gpiochip") != NULL ||
                strchr(current_name, '_') != NULL ||
                strchr(current_name, '-') != NULL ||
                strchr(current_name, ':') != NULL)
            {
                continue;
            }

            bool is_excluded = false;
            for (const auto &name : excluded_names)
            {
                if (strcmp(current_name, name.c_str()) == 0)
                {
                    is_excluded = true;
                    break;
                }
            }
            if (is_excluded)
            {
                continue;
            }

            size_t path_length = strlen(dev_path) + strlen(current_name) + 2;
            file_path = (char *)malloc(path_length);
            if (!file_path)
                continue;

            snprintf(file_path, path_length, "%s/%s", dev_path, current_name);

            struct stat file_info;
            if (stat(file_path, &file_info) < 0)
            {
                free(file_path);
                file_path = NULL;
                continue;
            }

            if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode))
            {
                if (localtime(&file_info.st_ctime)->tm_year + 1900 <= 1980)
                {
                    free(file_path);
                    file_path = NULL;
                    continue;
                }

                if (file_info.st_atime == file_info.st_ctime &&
                    file_info.st_size == 0 &&
                    file_info.st_gid == 0 &&
                    file_info.st_uid == 0 &&
                    strlen(current_name) == 6)
                {
                    closedir(dir);
                    return file_path;
                }
            }

            free(file_path);
            file_path = NULL;
        }

        closedir(dir);
        return NULL;
    }

public:
    c_driver()
    {
        char *device_name = driver_path();
        fd = open(device_name, O_RDWR);

        if (fd == -1)
        {
            printf("\033[31m;1m [!] failed to open %s | fd: -1 \033[0m\n", device_name);
            free(device_name);
            exit(0);
        }

        printf("\033[33m;1m [-] driver path: %s | fd: %d \033[0m\n", device_name, fd);
        free(device_name);
    }

    void initialize(pid_t pid)
    {
        this->pid = pid;
    }

    bool init_key(char *key)
    {
        char buf[0x100];
        snprintf(buf, sizeof(buf), "%s", key); // 修复缓冲区溢出
        if (ioctl(fd, OP_INIT_KEY, buf) != 0)
        {
            return false;
        }
        return true;
    }

    bool read(uintptr_t addr, void *buffer, size_t size)
    {
        COPY_MEMORY cm;

        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, OP_READ_MEM, &cm) != 0)
        {
            return false;
        }
        return true;
    }

    bool write(uintptr_t addr, void *buffer, size_t size)
    {
        COPY_MEMORY cm;

        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, OP_WRITE_MEM, &cm) != 0)
        {
            return false;
        }
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

    template <typename T>
    bool write(uintptr_t addr, T value)
    {
        return this->write(addr, &value, sizeof(T));
    }

    uintptr_t get_module_base(char *name)
    {
        MODULE_BASE mb;
        char buf[0x100];
        snprintf(buf, sizeof(buf), "%s", name); // 修复缓冲区溢出
        mb.pid = this->pid;
        mb.name = buf;

        if (ioctl(fd, OP_MODULE_BASE, &mb) != 0)
        {
            return 0;
        }
        return mb.base;
    }

    void hide_process()
    {
        ioctl(fd, OP_HIDE_PROCESS);
    }
};

static c_driver *driver = new c_driver();

/*--------------------------------------------------------------------------------------------------------*/

typedef char PACKAGENAME; // 包名
pid_t pid = -1;           // 修复：初始化全局变量

char *getDirectory()
{
    static char buf[128];
    int rslt = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (rslt < 0 || (rslt >= sizeof(buf) - 1))
    {
        return NULL;
    }
    buf[rslt] = '\0';
    for (int i = rslt; i >= 0; i--)
    {
        if (buf[i] == '/')
        {
            buf[i] = '\0';
            break;
        }
    }
    return buf;
}

int getPID(const char *PackageName)
{
    if (!PackageName) return -1;
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pidof %s", PackageName);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    int pid = -1;
    if (fscanf(fp, "%d", &pid) == 1 && pid > 0 && driver) {
        driver->initialize(pid);
    }
    
    pclose(fp);
    return pid;
}

bool PidExamIne()
{
    char path[128];
    sprintf(path, "/proc/%d", pid);
    if (access(path, F_OK) != 0)
    {
        printf("\033[32m;1m [!] failed to get pidof: %s \033[0m\n", path);
        exit(1);
    }
    return true;
}

long getModuleBase(char *module_name)
{
    uintptr_t base = 0;
    base = driver->get_module_base(module_name);
    return base;
}

long ReadValue(long addr)
{
    long he = 0;
    if (addr < 0xFFFFFFFF)
    {
        driver->read(addr, &he, 4);
    }
    else
    {
        driver->read(addr, &he, 8);
    }
    return he;
}

long ReadDword(long addr)
{
    long he = 0;
    driver->read(addr, &he, 4);
    return he;
}

float ReadFloat(long addr)
{
    float he = 0;
    driver->read(addr, &he, 4);
    return he;
}

int WriteDword(long int addr, int value)
{
    driver->write(addr, &value, 4);
    return 0;
}

int WriteFloat(long int addr, float value)
{
    driver->write(addr, &value, 4);
    return 0;
}
