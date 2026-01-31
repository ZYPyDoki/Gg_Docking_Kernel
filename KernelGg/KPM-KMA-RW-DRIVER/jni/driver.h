#ifndef DRIVER_H
#define DRIVER_H
#include <unistd.h>

// 驱动接口类，支持内核版本4.9~6.12
class Driver
{
  public:
    // 类ID，记录类起始地址，用于判断初始化是否成功，防止空指针异常/段错误，gid != 0 则表示初始化成功
	uint64_t gid;
	
	// 类ID,记录类结束地址,作用同上
	uint64_t uid;
	
    // 构造方法
	Driver() : gid(0), uid(0)
	{
		initkey("00aef3769a0dda443be62291c46f23b616d08fa77cce8629ab3b99f799aba7dcb85c0e24");
	}
	
    // 构析方法
    ~Driver();
            
	// 版本ID(密钥)，防止版本接口不一致导致内核崩溃，切勿随意更换，不需要手动调用
	void initkey(char *key);		    		
	
	// CPU亲和设置，cpuset(4)代表运行在CPU4上，cpuset(4,6)代表指定范围随机运行在CPU4-CPU6
	static void cpuset(uint8_t start, uint8_t end = 0);
    
    // 初始化pid，创建一个读写对象务必要初始一次
	void initpid(pid_t pid);	
	
	// 获取进程pid，传入进程(包名)，从内核层安全获取pid，当遇到加密混淆进程时可传入comm字段，参考/proc/pid/comm
	pid_t get_pid(char *name, char *comm = nullptr);
			
	// 获取模块地址，传入pid、接收指针、模块名、模块(长度)大小，当size=0时默认取模块第一行，从内核层安全获取模块地址，支持多线程
	bool get_module_base(pid_t pid, void *buffer, char *name, size_t size = 0);
	
	// 获取地址内存段，传入pid、接收指针(接收16字节)、目标地址、内存段权限、模块名、是否模糊(当前内存段不匹配则尽可能返回下一段匹配的内存段)，从内核层安全获取内存段，支持多线程
	bool get_module_base(pid_t pid, void *buffer, uintptr_t addr, char *prot, char *name, bool fuzzy);
	
	// 硬件级读取数据，直接读硬件地址，传入地址、接收指针、类型大小，指数级安全，由于不使用CPU缓存，效率相应降低，支持单进程多线程，支持 >= 5.4.x
	bool read_safe(uintptr_t addr, void *buffer, size_t size);
	
	// 结合read_safe和read优点的变种写法，传入地址、接收指针、类型大小，不使用CPU缓存，效率相应降低，支持单进程多线程，支持 >= 5.4.x
	bool read_fast(uintptr_t addr, void *buffer, size_t size);

    // 内核层读取数据，只读已映射到内核空间的地址，传入地址、接收指针、类型大小，支持单进程多线程，效率较高, 支持 >= 4.9.x
	bool read(uintptr_t addr, void *buffer, size_t size);
	
	// 内核层修改数据，只写已映射到内核空间的地址，传入地址、数据指针、类型大小，支持多线程
	bool write(uintptr_t addr, void *buffer, size_t size);

	// 模板方法，传入地址，返回地址上的值，支持多线程 PS：模板方法且仅适用于基本类型，至于为什么，多动脑想想...
	template <typename T> T read_safe(uintptr_t addr)
	{
		T res{};
		if (this->read_safe(addr, &res, sizeof(T)))
			return res;
		return 0;
	}		
	
	// 模板方法，传入地址，返回地址上的值，支持多线程
	template <typename T> T read_fast(uintptr_t addr)
	{
		T res{};
		if (this->read_fast(addr, &res, sizeof(T)))
			return res;
		return 0;
	}
		
	// 模板方法，传入地址，返回地址上的(数据)值，支持多线程
	template <typename T> T read(uintptr_t addr)
	{
		T res{};
		if (this->read(addr, &res, sizeof(T)))
			return res;
		return 0;
	}
	
	// 模板方法，传入地址，修改后的(数据)值，支持多线程
	template <typename T> bool write(uintptr_t addr, T value)
	{
		return this->write(addr, &value, sizeof(T));
	}
	
	// 获取模块方法的封装 传入pid、模块名称，返回模块起始地址
	uintptr_t get_module_base(pid_t pid, char *name)
	{
	    struct
	    {
	        uintptr_t start;
	        uintptr_t end;
	    } addr{};
	    get_module_base(pid, &addr, name);
	    return addr.start;
	}
	
	// 获取模块方法的封装 传入pid、模块名称、模块长度，返回模块起始地址
	uintptr_t get_module_base(pid_t pid, char *name, size_t size)
	{
	    struct
	    {
	        uintptr_t start;
	        uintptr_t end;
	    } addr{};
	    get_module_base(pid, &addr, name, size);
	    return addr.start;
	}
	
	/*
	    触摸事件有三个动作，按下、滑动、抬起
	    1.滑动事件：从A点滑动到B点，途径C点，则需要三个坐标，(Xa,Ya) (Xc,Yc) (Xb,Yb) 需要完成三个动作+延时：A点按下→A点滑动→延时→C点滑动→延时→B点滑动→B点抬起
	    2.单击事件：单击A点，则需要一个坐标，(Xa,Ya) 需要完成两个动作：A点按下→延时→A点抬起
		
	    该方法为触摸事件初始化，要使用触摸事件必须要进行初始化，参数为分辨率宽和高，如1080x2340,即传入1080,2340
	    以对象为最小单位，一个进程/线程可以使用一个或多个对象，但不能一个对象用于多线程。
	*/
	bool uinput_init(uint16_t width, uint16_t height);
	
	/*
	    传入屏幕坐标(x，y)，仅用于滑动事件，且最后一次调用后必须调用uinput_up进行抬起释放触摸点，否则将会出现卡屏/接触不良/无反应等等效果(参考你三根手指按在屏幕上，第四根滑后台，结果无法滑出)，若分辨率为1080x2340，竖屏下需要满足 0 < x && x < 1080, 0 < y && y < 2340 ，横屏下需要满足 0 < y && y < 1080, 0 < x && x < 2340
	    自带随机坐标、模拟真实手指(至少有一根手指在屏幕上，触摸事件才能生效)、反检测等等功能
	*/
	void uinput_move(int x, int y);
	
	// 传入屏幕坐标(x，y)，用于单击事件和滑动事件，特性同上
	void uinput_down(int x, int y);
	
	// 抬起并释放触摸点，任何一个触摸事件完成后都需要调用该方法，若出现卡屏/接触不良/无反应，都是触摸事件完成后没有及时调用该方法，若真出现卡屏现象，则只需要关闭触摸屏(息屏)即能恢复
	void uinput_up();
	
	// 从静态随机池中获取随机坐标，传入屏幕坐标x或者y，默认熵值25, 返回一个随机x或者y，并且有效，相比于一般随机(rand() % x)函数，该方法使用静态池(百万级别)模拟自然随机数，具有高效+高随机
	int uinput_rand(int val);
	
	// 该方法将重置随机池，传入屏幕坐标x或者y，传入一个熵，熵越大越混乱，取值1 ~ 100，若每次传入的熵值不同，则重置随机池 ，返回一个随机x/y，一个循环中输入一个固定熵值即可，否则效率将无限降低
	int uinput_rand(int val, int rand);
    
	// 解锁并释放资源 同一时间只能有一个用户在操作驱动(单进程多线程，最大支持 nproc/2 个线程) 进程结束后delete或者主动调用destroy释放资源，释放驱动,否则下次运行可能失败
	void destroy();		
};

// 这是扩展类 看不懂去AI
class Driver_EXT : public Driver
{
  public:
    // 扩展后支持读取[1,+∞]字节
    void read_safe_ext(uintptr_t addr, void *buffer, size_t size)
    {        
        uint8_t *src = (uint8_t *)buffer;
        while (size >= 3072)
        {
            read_safe(addr, src, 3072);            
            addr += 3072;
            src  += 3072;
            size -= 3072;            
        }
        read_safe(addr, src, size);
    }
    
    // 扩展后支持读取[1,+∞]字节
    void read_fast_ext(uintptr_t addr, void *buffer, size_t size)
    {        
        uint8_t *src = (uint8_t *)buffer;
        while (size >= 3072)
        {
            read_fast(addr, src, 3072);            
            addr += 3072;
            src  += 3072;
            size -= 3072;            
        }
        read_fast(addr, src, size);
    }
    
    // 扩展后支持读取[1,+∞]字节
    void read_ext(uintptr_t addr, void *buffer, size_t size)
    {        
        uint8_t *src = (uint8_t *)buffer;
        while (size >= 16384)
        {
            read(addr, src, 16384);            
            addr += 16384;
            src  += 16384;
            size -= 16384;            
        }
        read(addr, src, size);
    }
};

#endif
