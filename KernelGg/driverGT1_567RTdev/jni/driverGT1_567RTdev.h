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

		// 特定驱动文件名白名单
		const char *whitelist[] = { "wanbai", "CheckMe", "Ckanri", "lanran", "video188" };
		const int whitelist_count = sizeof(whitelist) / sizeof(whitelist[0]);
		
		const std::vector<std::string> excluded_names = {
			"binder", "common", "ashmem", "stdin", "stdout", "stderr"};

		struct dirent *entry;
		char *file_path = NULL;
		
		while ((entry = readdir(dir)) != NULL)
		{
			const char *current_name = entry->d_name;

			// 跳过.和..
			if (strcmp(current_name, ".") == 0 || strcmp(current_name, "..") == 0)
				continue;

			// 构建路径（提前构建用于白名单检查）
			size_t path_length = strlen(dev_path) + strlen(current_name) + 2;
			file_path = (char *)malloc(path_length);
			if (!file_path)
				continue;
			snprintf(file_path, path_length, "%s/%s", dev_path, current_name);

			// 优先检查特定文件名白名单
			bool is_whitelist = false;
			for (int i = 0; i < whitelist_count; i++)
			{
				if (strcmp(current_name, whitelist[i]) == 0)
				{
					printf("\033[33m;1m [-] whitelist match: %s \033[0m\n", file_path);
					closedir(dir);
					return file_path;
				}
			}

			// 优先级最高：驱动节点肯定是6个字符
			// 注意：白名单中的文件名可能长度不同，所以白名单检查在长度检查之前
			if (strlen(current_name) != 6)
			{
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 排除包含特定字符的设备名
			if (strstr(current_name, "gpiochip") != NULL ||
				strchr(current_name, '_') != NULL ||
				strchr(current_name, '-') != NULL ||
				strchr(current_name, ':') != NULL)
			{
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 排除特定名称
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
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 获取文件状态
			struct stat file_info;
			if (stat(file_path, &file_info) < 0)
			{
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 检查是否为字符设备或块设备
			if (!S_ISCHR(file_info.st_mode) && !S_ISBLK(file_info.st_mode))
			{
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 检查创建时间（跳过1980年前的文件）
			struct tm *timeinfo = localtime(&file_info.st_ctime);
			if (!timeinfo || (timeinfo->tm_year + 1900 <= 1980))
			{
				free(file_path);
				file_path = NULL;
				continue;
			}

			// 检查访问时间等于创建时间、大小为0、所有者用户组为root0
			if (file_info.st_atime == file_info.st_ctime &&
				file_info.st_size == 0 &&
				file_info.st_gid == 0 &&
				file_info.st_uid == 0 )
			{
				printf("\033[33m;1m [-] driver found: %s \033[0m\n", file_path);
				closedir(dir);
				return file_path;
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
