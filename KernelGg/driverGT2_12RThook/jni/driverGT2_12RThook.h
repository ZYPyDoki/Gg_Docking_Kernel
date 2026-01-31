#ifndef KERNEL_H
#define KERNEL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <iostream>
#include <cstdint>



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

    enum OPERATIONS
    {
        OP_INIT_KEY = 800,
        OP_READ_MEM = 601,
        OP_WRITE_MEM = 602,
        OP_MODULE_BASE = 603,
        OP_HIDE_PROCESS = 605,
    };

public:
    c_driver() {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) {
            perror("[-] 打开失败");
            exit(EXIT_FAILURE);
        }
    }

    ~c_driver() {
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
void hide_process() { ioctl(fd, OP_HIDE_PROCESS); }
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

typedef char PACKAGENAME;	// 包名
pid_t pid;	// 进程ID

float Kernel_v()
{
	const char* command = "uname -r | sed 's/\\.[^.]*$//g'";
	FILE* file = popen(command, "r");
	if (file == NULL) {
    	return 0.0f;
	}
	static char result[512];
	if (fgets(result, sizeof(result), file) == NULL) {
		return 0.0f;
	}
	pclose(file);
    result[strlen(result)-1] = '\0';
	return atof(result);
}

char *GetVersion(char* PackageName)
{
	char command[256];
	sprintf(command, "dumpsys package %s|grep versionName|sed 's/=/\\n/g'|tail -n 1", PackageName);
	FILE* file = popen(command, "r");
	if (file == NULL) {
		return NULL;
	}
	static char result[512];
	if (fgets(result, sizeof(result), file) == NULL) {
		return NULL;
	}
	pclose(file);
	result[strlen(result)-1] = '\0';
	return result;
}

uint64_t GetTime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return (ts.tv_sec*1000 + ts.tv_nsec/(1000*1000));
}

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

long GetModuleBaseAddr(char* module_name)
{
    long addr = 0;
    char filename[32];
    char line[1024];
    if (pid < 0)
    {
        snprintf(filename, sizeof(filename), "/proc/self/maps");
    }
    else
    {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }
    FILE *fp = fopen(filename, "r");
    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, module_name))
            {
				sscanf(line,"%lx-%*lx",&addr);
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

long getModuleBase(const char *module_name)
{
	long addr = 0;
	char module[64],lj[64],buff[256];
	char *part;
	bool bss = false;
	strcpy(module,module_name);
	part = strtok(module,":");
	strcpy(module,part);
	part = strtok(NULL,":");
	if(part)
	{
		if(strcmp(part,"bss")==0)
			bss = true;
	}
	if (pid <= 0)
		snprintf(lj, sizeof(lj), "/proc/self/maps");
	else
		snprintf(lj, sizeof(lj), "/proc/%d/maps", pid);
	FILE *fp = fopen(lj, "r");
	if(fp)
	{
		while(fgets(buff,sizeof(buff),fp))
		{
			if(strstr(buff,module)!=NULL)
			{
				if(strstr(buff,".so")!=NULL)
				{
					long add;
					sscanf(buff,"%lx-%*lx",&add);
					fgets(buff,sizeof(buff),fp);
					if(strstr(buff,module)==NULL){
						fgets(buff,sizeof(buff),fp);
					}
					if(strstr(buff,module)!=NULL)
					{
						if(bss){
							while(fgets(buff,sizeof(buff),fp))
							{
								if(strstr(buff,"[anon:.bss]")!=NULL)
								{
									sscanf(buff,"%lx-%*lx",&addr);
									break;
								}
							}
							break;
						}else{
							addr = add;
							break;
						}
					}
				}else{
					sscanf(buff,"%lx-%*lx",&addr);
					break;
				}
			}
		}
		fclose(fp);
	}
	return addr;
}

long ReadValue(long addr)
{
	long he=0;
	if (addr < 0xFFFFFFFF){
		driver->read(addr, &he, 4);
	}else{
		driver->read(addr, &he, 8);
		he=he&0xFFFFFFFFFFFF;
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

int *ReadArray(long addr)
{
	int *he = (int *) malloc(12);
	driver->read(addr, he, 12);
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
#endif
