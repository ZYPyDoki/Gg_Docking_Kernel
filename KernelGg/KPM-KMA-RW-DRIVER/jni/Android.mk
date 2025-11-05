# Android.mk (速度优先)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE    := KernelGg


LOCAL_SRC_FILES := lib5.cpp
LOCAL_LDLIBS    := -llog
LOCAL_LDLIBS += $(LOCAL_PATH)/driver.a

# --- 运行时性能优化 ---

# 1. 开启编译器优化
# -O3: 最高级别的优化，专注于代码执行速度。
# -Ofast: 更激进的优化，可能不严格遵守 IEEE 浮点数标准，但速度更快。
LOCAL_CPPFLAGS += -O3

# 2. 启用链接时优化
# 允许编译器在链接时跨模块进行优化，可以显著提升性能
LOCAL_LDFLAGS += -flto

# 3. 指定 CPU 架构特定优化
# 为 arm64-v8a 启用 NEON 和其他 ARMv8-A 特性
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_CPPFLAGS += -march=armv8-a
endif


include $(BUILD_SHARED_LIBRARY)