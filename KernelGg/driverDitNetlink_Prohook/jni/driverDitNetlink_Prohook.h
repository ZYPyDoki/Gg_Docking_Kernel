#include <iostream>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#define MAX_PAYLOAD 1048

// dit 3.3
struct Memory_uct
{
	int read_write;				// 读或者写
	int pid;
	uintptr_t addr;
	void *buffer;
	size_t size;
};

// dit pro 3.3
struct Ditpro_uct
{
	int read_write;				// 读或者写
	int pid;
	uintptr_t addr;
	void *buffer;
	size_t size;
	int wendi;					// 这个是判断只进行属于我们的操作,616
};


class drivers
{
  private:
	bool ditbool;
	int sock_fd;
	struct sockaddr_nl dest_addr;
	struct nlmsghdr *nlh;
	struct iovec iov;
	struct msghdr msg;
	pid_t pid;

  public:
	int drivers_byte;

	  drivers()
	{

		drivers_byte = 0;
		sock_fd = -1;
		nlh = NULL;

		sock_fd = socket(PF_NETLINK, SOCK_RAW, 17);

		if (sock_fd < 0)
		{
			if (sock_fd != -1)
			{
				close(sock_fd);
				sock_fd = -1;
			}
			if (nlh != NULL)
			{
				free(nlh);
				nlh = NULL;
			}
			this->drivers_byte = 0;	// 切换ditpro驱动
		}
		else
		{

			memset(&dest_addr, 0, sizeof(dest_addr));
			dest_addr.nl_family = AF_NETLINK;
			dest_addr.nl_pid = 0;	// 内核
			dest_addr.nl_groups = 0;

			nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
			if (!nlh)
			{
				perror("malloc");
				exit(1);
			}
			memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
			nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
			nlh->nlmsg_flags = 0;

			iov.iov_base = (void *)nlh;
			iov.iov_len = nlh->nlmsg_len;

			msg.msg_name = (void *)&dest_addr;
			msg.msg_namelen = sizeof(dest_addr);
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			this->ditbool = true;	// 通过
			this->drivers_byte = 1;	// 切换dit驱动
		}
	}

	bool read(uintptr_t addr, void *buffer, size_t size)
	{
		if ((int)this->drivers_byte == 1 && this->ditbool == true)
		{
			struct Memory_uct ptr;
			void *uctaddr = &ptr;
			ptr.addr = addr;
			ptr.buffer = buffer;
			ptr.pid = this->pid;
			ptr.read_write = 0x400;
			ptr.size = size;
			int bytelen = 8;
			char *source = (char *)&uctaddr;
			char *target = (char *)NLMSG_DATA(nlh);
			if (source == NULL || target == NULL)
				return false;
			while (bytelen--)
			{
				*target++ = *source++;
			}
			if (sendmsg(sock_fd, &msg, 0) < 0)
			{
				return false;
			}

		}
		else if ((int)this->drivers_byte == 0)
		{
			struct Ditpro_uct ptr;
			ptr.read_write = 0x400;
			ptr.addr = addr;
			ptr.buffer = buffer;
			ptr.wendi = 616;
			ptr.pid = this->pid;
			ptr.size = size;

			if (syscall(SYS_lookup_dcookie, &ptr))
			{
				printf("执行到了这里");
				return true;
			}
		}

		return true;
	}

	template < typename T > T read(uintptr_t addr)
	{
		T res;
		if (this->read(addr, &res, sizeof(T)))
			return res;
		return
		{
		};
	}

	int init_pid(pid_t pid)
	{
		this->pid = pid;
		return pid;
	}

	void initialize(pid_t pid)
	{
		this->pid = pid;
	}

	~drivers()
	{
		if (sock_fd != -1 && (int)this->drivers_byte == 1)
			close(sock_fd);
		if (nlh != NULL && (int)this->drivers_byte == 1)
			free(nlh);
	}

	int getPID(const char *packageName)
	{
		int id = -1;
		DIR *dir;
		FILE *fp;
		char filename[64];
		char cmdline[64];
		struct dirent *entry;
		dir = opendir("/proc");
		while ((entry = readdir(dir)) != NULL)
		{
			id = atoi(entry->d_name);
			if (id != 0)
			{
				sprintf(filename, "/proc/%d/cmdline", id);
				fp = fopen(filename, "r");
				if (fp)
				{
					fgets(cmdline, sizeof(cmdline), fp);
					fclose(fp);
					if (strcmp(packageName, cmdline) == 0)
					{
						return id;
					}
				}
			}
		}
		closedir(dir);
		return -1;
	}

	uintptr_t get_module_base(int pid, const char *module_name)
	{
		FILE *fp;
		long addr = 0;
		char *pch;
		char filename[64];
		char line[1024];
		snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
		fp = fopen(filename, "r");
		if (fp != NULL)
		{
			while (fgets(line, sizeof(line), fp))
			{
				if (strstr(line, module_name))
				{
					pch = strtok(line, "-");
					addr = strtoul(pch, NULL, 16);
					if (addr == 0x8000)
						addr = 0;
					break;
				}
			}
			fclose(fp);
		}
		return addr;
	}

};

static drivers *driver = new drivers();
