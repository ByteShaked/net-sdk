#pragma once

// 跨平台日志宏
// 鸿蒙用 HiLog，其他平台用 printf

#ifdef __OHOS__
    #include <hilog/log.h>
    #undef  LOG_DOMAIN
    #undef  LOG_TAG
    #define LOG_DOMAIN 0x3200
    #define LOG_TAG    "IM_NET"

    #define NET_LOG_DEBUG(fmt, ...) OH_LOG_DEBUG(LOG_APP, fmt, ##__VA_ARGS__)
    #define NET_LOG_INFO(fmt, ...)  OH_LOG_INFO(LOG_APP,  fmt, ##__VA_ARGS__)
    #define NET_LOG_WARN(fmt, ...)  OH_LOG_WARN(LOG_APP,  fmt, ##__VA_ARGS__)
    #define NET_LOG_ERROR(fmt, ...) OH_LOG_ERROR(LOG_APP, fmt, ##__VA_ARGS__)
#else
    #include <cstdio>
    #define NET_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define NET_LOG_INFO(fmt, ...)  printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
    #define NET_LOG_WARN(fmt, ...)  printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
    #define NET_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
