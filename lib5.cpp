/*
BSD 3-Clause License

Copyright (c) 2025, ZY彩虹海

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include "Log.h"
#define  PAGE_SIZE 4096
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

    enum OPERATIONS {
        OP_INIT_KEY = 800,
        OP_READ_MEM = 601,
        OP_WRITE_MEM = 602,
        OP_MODULE_BASE = 603
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

		ioctl(fd, OP_READ_MEM, &cm);
		return true;
	}

	bool write(uintptr_t addr, void *buffer, size_t size) {
		COPY_MEMORY cm;

		cm.pid = this->pid;
		cm.addr = addr;
		cm.buffer = buffer;
		cm.size = size;

		ioctl(fd, OP_WRITE_MEM, &cm);
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
};

static c_driver *driver = new c_driver();

/*--------------------------------------------------------------------------------------------------------*/

typedef char PACKAGENAME;	// 包名
pid_t pid;	// 进程ID

float Kernel_v()
{
	const char* command = "uname -r | sed 's/\\.[^.]*$//g'";
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





static uintptr_t get_module_base(const char* module_name) {
    FILE* file = fopen("/proc/self/maps", "r");
    if (!file) {
        return 0;
    }
    uintptr_t base = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, module_name) && !strstr(line, "r-xp")) {
            sscanf(line, "%lx", &base);
            break;
        }
    }
    fclose(file);
    return base;
}

static bool vm_readv(int pid, uintptr_t address, void* buffer, size_t size) {
    if (!address || !size || !pid || !buffer) {
        return false;
    }
    
    // 使用驱动读取目标进程内存
    return driver->read(address, buffer, size);

}

long my_syscall(long number, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {

    // 判断syscall读取方式拦截替换
    if (number == 270) { // SYS_process_vm_readv
        int target_pid = (int)arg1;
        struct iovec* local_iov = (struct iovec*)arg2;
        long local_count = arg3;
        struct iovec* remote_iov = (struct iovec*)arg4;
        long remote_count = arg5;

        LOGD("驱动读取: pid=%d, local_count=%ld, remote_count=%ld", target_pid, local_count, remote_count);
        
        // 内核驱动初始化目标包名进程pid
        driver->initialize(target_pid);

        ssize_t total_read = 0;
        for (long i = 0; i < local_count && i < remote_count; ++i) {
            uintptr_t remote_addr = (uintptr_t)remote_iov[i].iov_base;
            void* local_buffer = local_iov[i].iov_base;
            size_t size = local_iov[i].iov_len;

            // 使用驱动读取内存
			         driver->read(remote_addr, local_buffer, size);
			         //LOGE("读取成功: addr=0x%lx, size=%zu", remote_addr, size);
			         total_read += size;
		      }
        return total_read;
    }
    // 其他系统调用保持原样
    return syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
}



__attribute__((constructor)) static void init() {
    auto base = get_module_base("lib5.so");
    LOGD("Module base address: %lx", base);
    for (long i = 0; ; ++i) {
        auto addr = base + i * sizeof(uintptr_t);
        if (*(uintptr_t*)addr == (uintptr_t)syscall) {
            LOGD("Found syscall at offset %p syscall %p", addr, *(uintptr_t*)addr);
            //对齐
            mprotect((void*)(addr & ~(PAGE_SIZE - 1)), PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
            *(uintptr_t*)(addr) = (uintptr_t)my_syscall;
            break;
        }
    }
}
