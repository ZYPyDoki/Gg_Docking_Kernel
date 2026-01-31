
#ifndef _KERNEL_H_
#define _KERNEL_H_
#include <cstdint>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sys/syscall.h>

#define MAJOR 0
#define MINOR 11
#define PATCH 1

#define SUPERCALL_HELLO_ECHO "hello1158"

// #define __NR_supercall __NR3264_truncate // 45
#define __NR_supercall 45

#define SUPERCALL_HELLO 0x1000
#define SUPERCALL_KLOG 0x1004

#define SUPERCALL_KERNELPATCH_VER 0x1008
#define SUPERCALL_KERNEL_VER 0x1009


#define SUPERCALL_KPM_LOAD 0x1020
#define SUPERCALL_KPM_UNLOAD 0x1021
#define SUPERCALL_KPM_CONTROL 0x1022

#define SUPERCALL_KPM_NUMS 0x1030
#define SUPERCALL_KPM_LIST 0x1031
#define SUPERCALL_KPM_INFO 0x1032


#define SUPERCALL_HELLO_MAGIC 0x11581158


static inline long hash_key(const char *key)
{
    long hash = 1000000007;
    for (int i = 0; key[i]; i++) {
        hash = hash * 31 + key[i];
    }
    return hash;
}
// be 0a04
static inline long hash_key_cmd(const char *key, long cmd)
{
    long hash = hash_key(key);
    return (hash & 0xFFFF0000) | cmd;
}

// ge 0a05
static inline long ver_and_cmd( long cmd)
{
    uint32_t version_code = (MAJOR << 16) + (MINOR << 8) + PATCH;
    return ((long)version_code << 32) | (0x1158 << 16) | (cmd & 0xFFFF);
}

static inline long compact_cmd(const char *key, long cmd)
{
    long ver = syscall(__NR_supercall, key, ver_and_cmd( SUPERCALL_KERNELPATCH_VER));
    if (ver >= 0xa05) return ver_and_cmd( cmd);
    return hash_key_cmd(key, cmd);
}


static inline long sc_hello(const char *key)
{
    if (!key || !key[0]) return -EINVAL;
    long ret = syscall(__NR_supercall, key, compact_cmd(key, SUPERCALL_HELLO));
    return ret;
}

static inline bool sc_ready(const char *key)
{
    return sc_hello(key) == SUPERCALL_HELLO_MAGIC;
}

class kernel
{

private:
    struct kpm_read
    {
        uint64_t key;
        int pid;
        int size;
        uint64_t addr;
        void *buffer;

    };

    struct kpm_mod
    {
        uint64_t key;
        int pid;
        char *name;
        uintptr_t base;

    };
    uint64_t key_vertify;
    unsigned int cmd_read;
    unsigned int cmd_write;
    unsigned int cmd_mod;
    struct kpm_read kread;
    struct kpm_mod kmod;
    
public:
  bool kpm_Status=false;
    
    bool cmd_ctl(std::string SuperCallKey){
        if(SuperCallKey.empty()) return false;
        std::string key_cmd = "get_key";
        char  buf[256] = {0};
        long ret = syscall(__NR_supercall, SuperCallKey.c_str() , compact_cmd(SuperCallKey.c_str(), SUPERCALL_KPM_CONTROL), "kpm_kread", key_cmd.c_str(), buf, 256);
        if(ret<0) return false;
        std::string str_buf = std::string(buf);
        //std::cout<<"str_buf: "<<str_buf<<std::endl;
        int pos = str_buf.find("-");
        if(pos == std::string::npos) return false;
        key_vertify = std::stoull(str_buf.substr(0,pos),nullptr,16);
        cmd_read = std::stoull(str_buf.substr(pos+1),nullptr,16);
        init(cmd_read,key_vertify);
        kpm_Status=true;
        return true;
    }

    kernel(){};

    void init(uint64_t cmd,uint64_t key){
        cmd_read = cmd;//十六进制
        cmd_write = cmd + 1;
        cmd_mod = cmd + 2;
        kread.key = key;//十六进制
        kmod.key = key;
    };

    void set_pid(int pid){
        kread.pid = pid;
        kmod.pid = pid;
    }

    bool read(uint64_t addr, void *buffer, int size){
        kread.addr = addr;
        kread.size = size;
        kread.buffer = buffer;
        int ret = ioctl(-1,cmd_read,&kread);
      //  sycall(entry,-1, cmd_read,&kread);
        //ioctl(-1,cmd_read,&kread);
         if(ret<0){
         //  std::cout<<"read error"<<std::endl;

        return false;
         }
        return true;
    }
    template<typename T>
    T read(uint64_t addr){
        T data= 0;
        kread.addr = addr ;
        kread.size = sizeof(T);
        kread.buffer = &data;
        int ret = ioctl(-1,cmd_read,&kread);
        //syscall(entry,-1, cmd_read, &kread);
        // ioctl(-1,cmd_read,&kread);
        //if(ret<0){
            //std::cout<<"read error maybe pa false"<<std::endl;
            //return 0;
        //}
        return data;
    }

    bool write(uint64_t addr,  void *buffer, int size){
        kread.addr = addr & 0xffffffffffff;
        kread.size = size;
        kread.buffer = buffer;
        int ret = ioctl(-1,cmd_write,&kread);
        //syscall(entry,-1, cmd_write,&kread);
        //ioctl(-1,cmd_write,&kread);
         if(ret<0){
        //     //std::cout<<"write error"<<std::endl;
        return false;
         }
        return true;
    }


    uintptr_t get_module_base(std::string name){
        kmod.name = const_cast<char*>(name.c_str());
        int ret = ioctl(-1,cmd_mod,&kmod);
        //syacall(entry,-1, cmd_mod,&kmod);
        //ioctl(-1,cmd_mod,&kmod);
        if(ret<0){
            std::cout<<"get_mod_base error" << std::hex << kmod.base <<
                     ", pid:"<< std::dec << kmod.pid <<std::endl;
            return 0;
        }
        return kmod.base;
    }


};
kernel kdriver; //栈分配
#endif
