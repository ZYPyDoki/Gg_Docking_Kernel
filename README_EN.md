<p align="center">
  <img src="https://img.shields.io/badge/Android-ARM64%20Only-brightgreen?style=flat-square&logo=android" alt="Platform">
  <img src="https://img.shields.io/badge/Kernel-10%2B%20Drivers-blue?style=flat-square&logo=linux" alt="Drivers">
  <img src="https://img.shields.io/badge/Hook-Universal%20Syscall%20270-red?style=flat-square&logo=hook" alt="Hook">
</p>

<h1 align="center">🚀 Gg_Docking_Kernel</h1>
<p align="center"><b>Universal Kernel Driver Hook Framework for Android</b></p>
<p align="center">Enable any executable to interface with kernel drivers | Universal Syscall 270 Hook Framework supporting 10+ drivers</p>

<p align="center">
  ✈️ Telegram: <a href="https://t.me/ZYChats">@ZYChats</a> | 
  🌐 <a href="README.md">中文</a>
</p>

---

## 🔥 Core Features

### 1. Universal Hook Architecture (New)
**Breakthrough global hook capability** — No longer limited to `lib5.so`, now capable of hooking **any executable binary** in the system as long as it calls syscall 270 (`process_vm_readv`).

- 🎯 **Third-party auxiliary kernelization**: Provides kernel-level memory reading capabilities for auxiliary tool authors who haven't updated their drivers
- 🔑 **Key pre-update**: Driver author updated the key but the main program hasn't followed up? Compile the hook layer in advance to seamlessly switch to the new driver
- 🛡️ **Zero-intrusion modification**: No need to modify target application source code, dynamically replace system call implementation for memory reading

### 2. Ten Driver Ecosystem (New)
Fully supports **KPM Kernel Persistent Modules** and **KO Temporary Drivers** two major categories, totaling 10 industry mainstream driver solutions.

---

## 📦 Supported Driver Types

### KPM Kernel Persistent Modules
Stable and efficient, suitable for long-term usage scenarios.

| Driver Name | Author/Source | Characteristics | Status |
|:---|:---|:---|:---|
| `KPM-KMA-RW-DRIVER` | King777 | The oldest KPM memory read/write module, iterated through dozens of versions, most stable, compatible, efficient, and stealthy. Cons: Server backend often goes down/discontinued. | Industry Benchmark |
| `ditProKpm` | BarbaraTos Author | New work from early 2026, derivative solution | Emerging Mainstream |
| `KpmApReadioctl` | Ni (溺) | Early Apatch-read veteran solution | Classic & Stable |
| `KpmMemoryioctlhook` | Anonymous | Open-source interface, convenient for integration development | Open & Compatible |
| `KpmTearioctlhook` | AYe (阿夜) & TearHeart (泪心) | KPM memory read/write module jointly developed in 2026 by the famous FastCan base address scanning tool author and the famous Honor of Kings tool author. | Strong Alliance |

### KO Temporary Drivers (Kernel Object)
Temporarily loaded, high flexibility, suitable for specific scenarios.

| Driver Name | Author/Source | Interface Type | Characteristics |
|:---|:---|:---|:---|
| `driverParadise` | Cycle1337 | Hook | Based on open-source Hook iteration optimization, Valorant/Delta Force tool author |
| `driverDitNetlink` | BarbaraTos | Netlink | Developed around 2024, foundation driver for LianLiZhi solution |
| `driverQX114` | SnowTech (雪花科技) | Snowflake | Earliest and most widespread KO driver solution on the market, modified based on JiangWan's open-source foundation |
| `driverGT1_567RTdev` | WangChuan/RT | Dev Node | GT 1.5/1.6/1.7 + RT drivers, strong compatibility |
| `driverGT2_12RThook` | WangChuan/RT | Hook | GT 2.1/2.2 + RT drivers, high stealth |

---

## 🏗️ Technical Architecture

### Traditional Solution vs Universal Hook
```

Traditional (lib5.cpp)          Universal Hook (lib5native_hook.cpp)
│                              │
▼                              ▼
┌──────────────┐               ┌──────────────────┐
│  Only Hook   │               │  Hook Any ELF    │
│  lib5.so     │               │  syscall 270     │
│process_vm_readv              └──────────────────┘
└──────────────┘

│                              │
└──────────┬───────────────────┘
▼
┌────────────────┐
│  10 Kernel     │
│  Driver Switch │
│  (KPM/KO Dual) │
└────────────────┘
│
▼
┌────────────────┐
│  Hardware-level│
│  Memory Read   │
│  Bypass Userspace Detection │
└────────────────┘

```

### Code Evolution
- **Legacy (`lib5.cpp`)**: GOT table hook targeting `lib5.so.primary`, single file hard-coded
- **New (`lib5native_hook.cpp`)**: 
  - Uses `dlsym(RTLD_NEXT, "syscall")` for global symbol interception
  - Supports dynamic PID switching and thread-safe initialization (double-checked locking)
  - Memory merge algorithm optimization, reducing driver context switching
  - Pure AArch64 assembly fallback, ensuring stability

---

## 🛠️ Quick Start

### Option A: Using Precompiled Versions
1. Enter the `Compiled_finished_product` folder
2. Select the subfolder corresponding to your driver
3. **Move all contents** and replace them into the target application's private directory

**Standard Path Example**:
```bash
/data/user/0/catch_.me_.if_.you_.can_/files/GG-7E6k/
```

> ⚠️ Note: Each GG Modifier has different package names, so their private directory names will also differ. Don't copy-paste directly; find the actual path according to your specific situation.

Option B: Universal Hook Injection (New)
Suitable for injecting kernel driver capabilities into third-party applications:

```bash
# Get the directory where the script is located
SCRIPT_DIR="$(dirname "$0")"

export LD_LIBRARY_PATH="${SCRIPT_DIR}:${LD_LIBRARY_PATH}"
export LD_PRELOAD="${SCRIPT_DIR}/libKernelGg.so"

# Your executable binary file path (e.g., ./main in relative path under current directory)
exec "./main" "$@"
```

---

🔨 Build from Source

Environment Preparation
- Android NDK (recommended r25+ or r28c)
- Linux/macOS/Termux build environment
- AArch64 architecture support (`__aarch64__`)

Build Steps

1. Source Configuration

```bash
# After cloning the project, enter according to the target driver folder name you want to compile
git clone https://github.com/ZYPyDoki/Gg_Docking_Kernel
```

2. Modify Build Configuration
Edit `build.sh`, replace the NDK path:

```bash
# Original path
/data/user/0/com.termux/files/home/android-ndk/ndk/29.0.14206865/ndk-build
# Change to your path, for example:
/home/user/android-ndk/ndk/25.2.9519653/ndk-build
```

3. Execute Build

```bash
cd /path/to/source
chmod +x build.sh
./build.sh
```

4. Get Output
- Output file: `libs/arm64-v8a/libKernelGg.so`
- Ready to use when "Project compilation completed" message appears

---

💡 Implementation Principles

> Credits: Original concept by enen (大牛)

Before GG Modifier loads its native file I/O library `lib5.so`, preemptively load a shared library we prepared. After loading, let GG load its original `lib5.so`. Through this method, we can Hook system call number 270, thereby replacing file read operations in `lib5.so` with reads performed through the kernel driver.

Memory Read Flow Hijacking
1. Injection Timing: Before the target application loads native libraries, preemptively load `libKernelGg.so`
2. Symbol Hijacking: Intercept `syscall` symbol via `RTLD_NEXT`, redirect to custom implementation
3. Call Conversion: When syscall number 270 (`__NR_process_vm_readv`) is detected:
   - Parse `iovec` structure parameters
   - Merge contiguous memory blocks (optimization strategy)
   - Call kernel driver interface instead of standard system call
4. Transparent Fallback: Non-target calls are directly passed through to the original syscall, ensuring other application functions work normally

Why Choose Syscall 270?
`process_vm_readv` is the standard Linux cross-process memory reading interface, commonly used by game modifiers and auxiliary tools for memory searching. Hooking this call point achieves:
- ✅ Transparent replacement of underlying implementation
- ✅ Zero modification to upper-layer business logic
- ✅ Native support for all programs calling this interface

---

📚 NDK Installation Guide

Video Tutorial: [Bilibili Tutorial](https://b23.tv/ljTKIDV)

One-click Installation (Linux aarch64):

```bash
cd && tar -xvf android-ndk-r28c-aarch64-linux-android.tar.xz && rm -rf android-ndk-r28c-aarch64-linux-android.tar.xz && mkdir -p android-ndk/ndk && mv android-ndk-r28c 28.2.13676358 && mv 28.2.13676358 android-ndk/ndk/ && ln -s $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64 && ln -s $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-x86_64 && echo '✅ NDK Installation Finished!'
```

---

⚠️ Important Notes

Driver Deployment Permissions (Critical)
- Kernel KO drivers with load.sh: Must extract to root directory (e.g., `/data/local/`), grant `777` permissions, owner user group `root:0`
- File Permissions: After replacing the Hook library, be sure to set permissions to `777`, owner user group root:0 or target application

Version Compatibility

Version	Target File	File Size	Storage Location	
v96	`lib5.so.primary`	3.9 MB	Project root directory	
v101	`lib5.so.primary`	4.4 MB	`101version/` folder	

Universal Hook Special Notes
- Scope: Due to using `RTLD_NEXT`, ensure this library loads before the target library
- Thread Safety: Implemented `pthread_mutex` protection for PID switching, safe for multi-threaded environments
- Architecture Limitation: Strictly limited to `__aarch64__`, 32-bit devices not supported

---

📜 Changelog

January 2026 - Major Update v3.0
- ✨ Added 5 new driver supports: Expanded to 10 mainstream driver ecosystem
- 🚀 Universal Hook Architecture: Support hooking any executable, no longer limited to lib5.so
- ⚡ Performance Optimization: Added memory block merge algorithm, reducing driver context switching overhead
- 🛡️ AArch64 Assembly Fallback: Automatic fallback to inline assembly implementation when syscall fails

---

🤝 Contributors & Acknowledgments

Kernel Driver Module Developers List

> (Jokingly: The arrest list if things go wrong)
Randomly ordered
- [Dit](https://t.me/bbtswd)
- [GT](https://t.me/GTnewdriver)
- [RT](https://t.me/RTdrivers)
- [Paradise](https://t.me/ParadiseKMA)
- [QX](https://t.me/qxwmqd)
- [KMA](https://discord.gg/y6nzS7TmuM)
- [Ni (溺)](https://github.com/niqiuqiux)
- [AYe (阿夜)](https://github.com/AYssu)
- [TearHeart (泪心)](https://t.me/TearGame)

> Please comply with local laws and regulations. This tool is for educational and research purposes only.

💖 Sponsorship
If this project helps you, welcome to [Buy me a coffee ☕](https://afdian.com/a/zypyd) `https://afdian.com/a/zypyd` to support the project's continuous development. Thank you for your support! Your sponsorship will motivate me to keep improving this project.
