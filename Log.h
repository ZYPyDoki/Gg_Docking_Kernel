#ifndef LOGGER_H
#define LOGGER_H

#include <android/log.h>


#ifndef NDEBUG
#define TAG "日志"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))

#define LOGD(fmt, ...)                                                                             \
__android_log_print(ANDROID_LOG_DEBUG, TAG,                                                \
"%s:%d#%s"                                                                 \
": " fmt,                                                                  \
__FILE_NAME__, __LINE__, __PRETTY_FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

#define LOGA(...) LOGD(LOGADDR_FORMAT(__VA_ARGS__), __VA_ARGS__)

#define LOGADDR_FORMAT(...) LOGADDR_FORMAT_IMPL(__VA_ARGS__)
#define LOGADDR_FORMAT_IMPL(...) LOGADDR_FORMAT_SELECT(__VA_ARGS__, LOGADDR_FORMAT_5, LOGADDR_FORMAT_4, LOGADDR_FORMAT_3, LOGADDR_FORMAT_2, LOGADDR_FORMAT_1)(__VA_ARGS__)
#define LOGADDR_FORMAT_SELECT(_1, _2, _3, _4, _5, NAME, ...) NAME

#define LOGADDR_FORMAT_1(addr1) #addr1 ": %p"
#define LOGADDR_FORMAT_2(addr1, addr2) #addr1 ": %p, " #addr2 ": %p"
#define LOGADDR_FORMAT_3(addr1, addr2, addr3) #addr1 ": %p, " #addr2 ": %p, " #addr3 ": %p"
#define LOGADDR_FORMAT_4(addr1, addr2, addr3, addr4) #addr1 ": %p, " #addr2 ": %p, " #addr3 ": %p, " #addr4 ": %p"
#define LOGADDR_FORMAT_5(addr1, addr2, addr3, addr4, addr5) #addr1 ": %p, " #addr2 ": %p, " #addr3 ": %p, " #addr4 ": %p, " #addr5 ": %p"

#else
#define LOGI(...) NULL
#define LOGW(...) NULL
#define LOGD(...) NULL
#define LOGE(...) NULL
#define LOGA(...) NULL
#endif

#endif