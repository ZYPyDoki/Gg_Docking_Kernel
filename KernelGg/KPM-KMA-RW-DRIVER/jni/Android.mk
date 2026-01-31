LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := KernelGg
LOCAL_SRC_FILES := lib5native_hook.cpp

LOCAL_LDLIBS    := -llog -ldl

# 性能 + 稳定性：O3 + LTO + 去除无用段
LOCAL_CPPFLAGS  += -O3 -DNDEBUG -fvisibility=hidden -fdata-sections -ffunction-sections
LOCAL_CPPFLAGS  += -std=c++17 -fno-exceptions -fno-rtti

LOCAL_CPPFLAGS  += -flto
LOCAL_LDFLAGS   += -flto -Wl,--gc-sections -Wl,--exclude-libs,ALL

# 可选：减少符号（发布建议开启；调试可注释）
LOCAL_LDFLAGS   += -Wl,--strip-all


LOCAL_LDLIBS += $(LOCAL_PATH)/driver.a


include $(BUILD_SHARED_LIBRARY)
