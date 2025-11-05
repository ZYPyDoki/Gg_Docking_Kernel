
# 🚀 Gg_Docking_Kernel
> Enable GG Modifier to interface with kernel drivers for kernel-level memory reading, bypassing most search detections.
---
## 📦 Supported Drivers
| Driver Name | Description | Feature |
| :--- | :--- | :--- |
| `KPM-KMA-RW-DRIVER` | Kernel module for kernel managers | Hardware-level reading |
| `driverGT1_567RTdev` | Dev solution for GT and RT drivers | High compatibility |
| `driverGT2_12RThook` | Hook solution for GT and RT drivers | High stealth |
| `driverDitNetlink_Prohook` | Customized dedicated driver | Professional custom |
---
## 🛠️ Using Pre-compiled Versions
1.  Navigate to the `Compiled_finished_product` directory.
2.  Select the folder corresponding to your desired driver.
3.  Move and overwrite all contents to GG Modifier's private directory.
    **Example path:**
    ```
    /data/user/0/com.apocalua.run/files/AppHidden-85uS/
    ```
> **⚠️ Note**: Different GG Modifier versions may have different package names and thus different private directory paths.
---
## 🔨 Compiling the Project
### 📋 Prerequisites
-   A configured Android NDK environment.
### 📝 Compilation Steps
**Step 1: Prepare Source Code**
-   Download and extract the source code for the desired driver into an empty directory.
-   Modify the NDK path in the `build.sh` file, replacing it with your local `ndk-build` installation path.
    **Example:**
    ```sh
    # Replace this path
    /data/user/0/com.termux/files/home/android-ndk/ndk/29.0.14206865/ndk-build
    # with your own, for example:
    /home/user/android-ndk/ndk/25.2.9519653/ndk-build
    ```
**Step 2: Enter Directory**
-   Use the `cd` command to enter the source directory.
    ```sh
    cd /path/to/your/source/folder
    ```
**Step 3: Execute Compilation**
-   Run the build script.
    ```sh
    ./build.sh
    ```
**Final Step: Retrieve the Build**
-   Wait for the "Compilation complete" message. The compiled file, `libKernelGg.so`, will be generated in the `libs/arm64-v8a/` directory, located in the same directory as `build.sh`.
---
## 💡 Implementation Principle
> Based on enen's concept.
>
> Before GG Modifier loads its native read-write library `lib5.so`, we preload a custom shared library. After our library is loaded, GG proceeds to load its original `lib5.so`. This method allows us to **hook system call number 270**, effectively replacing the file read operations within `lib5.so` with reads performed by the kernel driver.
---
## 📚 NDK Installation Guide
**Video Tutorial:** [https://b23.tv/ljTKIDV](https://b23.tv/ljTKIDV)
**One-click installation command (for aarch64 Linux):**
```sh
cd && tar -xvf android-ndk-r28c-aarch64-linux-android.tar.xz && rm -rf android-ndk-r28c-aarch64-linux-android.tar.xz && mkdir android-ndk && mkdir android-ndk/ndk && mv android-ndk-r28c 28.2.13676358 && mv 28.2.13676358 android-ndk/ndk/ && ln -s $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64 && ln -s $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-aarch64 $HOME/android-ndk/ndk/28.2.13676358/prebuilt/linux-x86_64 && echo 'Installation Finished. Ndk has been installed successfully!'
```
---
## ⚠️ Important Notes
-   **GT Version Drivers**: Must be extracted to a root directory (e.g., `/data/local/`) with `777` permissions and `root:0` ownership.
-   **File Permissions**: After replacing files, ensure their permissions are set to `777` and their ownership is changed to GG Modifier's user group.
-   **Version Differences**:
    -   **GG 96 Version**: The `lib5.so.primary` file size is approximately **3.9 MB**.
    -   **GG 101 Version**: Use the corresponding `lib5.so.primary` from the `101version` directory in this project. The file size is approximately **4.4 MB**.
```
