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

class c_driver {
private:
    int has_upper = 0;
    int has_lower = 0;
    int has_symbol = 0;
    int has_digit = 0;
    int fd;
    pid_t pid;

    typedef struct _COPY_MEMORY {
        pid_t pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    } COPY_MEMORY, *PCOPY_MEMORY;

    typedef struct _MODULE_BASE {
        pid_t pid;
        char* name;
        uintptr_t base;
    } MODULE_BASE, *PMODULE_BASE;

    struct process {
        pid_t process_pid;
        char process_comm[15];
    };

    enum OPERATIONS {
        OP_INIT_KEY = 0x800,
        OP_READ_MEM = 0x801,
        OP_WRITE_MEM = 0x802,
        OP_MODULE_BASE = 0x803,
        OP_HIDE_PROCESS = 0x804,
    };

    int symbol_file(const char *filename) {
        //判断文件名是否含小写并且不含大写不含数字不含符号
        int length = strlen(filename);
        for (int i = 0; i < length; i++) {
            if (islower(filename[i])) {
                has_lower = 1;
            } else if (isupper(filename[i])) {
                has_upper = 1;
            } else if (ispunct(filename[i])) {
                has_symbol = 1;
            } else if (isdigit(filename[i])) {
                has_digit = 1;
            }
        }
        return has_lower && !has_upper && !has_symbol && !has_digit;
    }

    char *driver_path() {
        // 打开目录
        const char *dev_path = "/dev";
        DIR *dir = opendir(dev_path);
        if (dir == NULL) {
            printf("无法打开/dev目录\n");
            return NULL;
        }

        const char *files[] = { "wanbai", "CheckMe", "Ckanri", "lanran","video188"};
        struct dirent *entry;
        char *file_path = NULL;
        while ((entry = readdir(dir)) != NULL) {
            // 跳过当前目录和上级目录
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            size_t path_length = strlen(dev_path) + strlen(entry->d_name) + 2;
            file_path = (char *)malloc(path_length);
            snprintf(file_path, path_length, "%s/%s", dev_path, entry->d_name);
            for (int i = 0; i < 5; i++) {
                if (strcmp(entry->d_name, files[i]) == 0) {
                    printf("驱动文件：%s\n", file_path);
                    closedir(dir);
                    return file_path;
                }
            }

            // 获取文件stat结构
            struct stat file_info;
            if (stat(file_path, &file_info) < 0) {
                free(file_path);
                file_path = NULL;
                continue;
            }

            // 跳过gpio接口
            if (strstr(entry->d_name, "gpiochip") != NULL) {
                free(file_path);
                file_path = NULL;
                continue;
            }

            // 检查是否为驱动文件
            if ((S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode))
                    && strchr(entry->d_name, '_') == NULL && strchr(entry->d_name, '-') == NULL && strchr(entry->d_name, ':') == NULL) {
                // 过滤标准输入输出
                if (strcmp(entry->d_name, "stdin") == 0 || strcmp(entry->d_name, "stdout") == 0
                        || strcmp(entry->d_name, "stderr") == 0) {
                    free(file_path);
                    file_path = NULL;
                    continue;
                }

                size_t file_name_length = strlen(entry->d_name);
                time_t current_time;
                time(&current_time);
                int current_year = localtime(&current_time)->tm_year + 1900;
                int file_year = localtime(&file_info.st_ctime)->tm_year + 1900;
                //跳过1980年前的文件
                if (file_year <= 1980) {
                    free(file_path);
                    file_path = NULL;
                    continue;
                }

                time_t atime = file_info.st_atime;
                time_t ctime = file_info.st_ctime;
                // 检查最近访问时间和修改时间是否一致并且文件名是否是symbol文件
                if (atime == ctime)/* && symbol_file(entry->d_name)*/ {
                    //检查mode权限类型是否为S_IFREG(普通文件)和大小还有gid和uid是否为0(root)并且文件名称长度在7位或7位以下
                    if ((file_info.st_mode & S_IFMT) == 8192 && file_info.st_size == 0
                            && file_info.st_gid == 0 && file_info.st_uid == 0 && file_name_length <= 9) {
                        printf("驱动文件：%s\n", file_path);
                        closedir(dir);
                        return file_path;
                    }
                }
            }
            free(file_path);
            file_path = NULL;
        }
        closedir(dir);
        return NULL;
    }
public:
    c_driver() {
        char *device_name = driver_path();
        fd = open(device_name, O_RDWR);

        if (fd == -1) {
            printf("[-] open driver failed\n");
            free(device_name);
            exit(0);
        }
        free(device_name);
    }

    ~c_driver() {
        //wont be called
        if (fd > 0)
            close(fd);
    }

    void initialize(pid_t pid) {
        this->pid = pid;
    }

    bool init_key(char* key) {
        char buf[0x100];
        strcpy(buf,key);
        if (ioctl(fd, OP_INIT_KEY, buf) != 0) {
            return false;
        }
        return true;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        COPY_MEMORY cm;

        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, OP_READ_MEM, &cm) != 0) {
            return false;
        }
        return true;
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        COPY_MEMORY cm;

        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, OP_WRITE_MEM, &cm) != 0) {
            return false;
        }
        return true;
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res;
        if (this->read(addr, &res, sizeof(T)))
            return res;
        return {};
    }

    template <typename T>
    bool write(uintptr_t addr,T value) {
        return this->write(addr, &value, sizeof(T));
    }

    uintptr_t get_module_base(char* name) {
        MODULE_BASE mb;
        char buf[0x100];
        strcpy(buf,name);
        mb.pid = this->pid;
        mb.name = buf;

        if (ioctl(fd, OP_MODULE_BASE, &mb) != 0) {
            return 0;
        }
        return mb.base;
    }

    void hide_process() {
        ioctl(fd, OP_HIDE_PROCESS);
    }


};

static c_driver *driver = new c_driver();

/*--------------------------------------------------------------------------------------------------------*/

typedef char PACKAGENAME;	// 包名
pid_t pid;	// 进程ID

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

int getPID(char* PackageName)
{
    FILE* fp;
    char cmd[0x100] = "pidof ";
    strcat(cmd, PackageName);
    fp = popen(cmd,"r");
    fscanf(fp,"%d", &pid);
    pclose(fp);
    if (pid > 0)
    {
        driver->initialize(pid);
    }
    return pid;
}

bool PidExamIne()
{
    char path[128];
    sprintf(path, "/proc/%d",pid);
    if (access(path,F_OK) != 0)
    {
        printf("\033[31;1m");
        puts("获取进程PID失败!");
        exit(1);
    }
    return true;
}

long getModuleBase(char* module_name)
{
    uintptr_t base=0;
    base = driver->get_module_base(module_name);
    return base;
}

long ReadValue(long addr)
{
    long he=0;
    if (addr < 0xFFFFFFFF) {
        driver->read(addr, &he, 4);
    } else {
        driver->read(addr, &he, 8);
    }
    return he;
}

long ReadDword(long addr)
{
    long he=0;
    driver->read(addr, &he, 4);
    return he;
}

float ReadFloat(long addr)
{
    float he=0;
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