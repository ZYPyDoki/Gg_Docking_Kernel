# Application.mk

# 指定目标 CPU 架构
# 对应 CMake 中的 set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
APP_ABI := arm64-v8a

# 指定最低支持的 Android API Level
# 对应 CMake 中的 set(ANDROID_PLATFORM android-28)
# 注意：ndk-build 使用数字，而 CMake 的 android.toolchain.cmake 也接受数字
APP_PLATFORM := android-28

# (可选) 指定要使用的 C++ 标准
# 这是一种更现代的替代 LOCAL_CPPFLAGS 的方式，效果相同
APP_CPPFLAGS := -std=c++20

# (可选) 如果你的项目包含 C++ 代码，建议启用 STL
APP_STL := c++_shared

APP_OPTIM := release