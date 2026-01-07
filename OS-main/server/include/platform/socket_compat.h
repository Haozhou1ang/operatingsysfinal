#pragma once

// 跨平台 Socket 兼容层
// 统一 Windows 和 Unix/Linux/macOS 的 socket API 差异

#ifdef _WIN32
    // Windows 平台
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <cstdio>
    #pragma comment(lib, "ws2_32.lib")
    
    // Windows 使用 SOCKET 类型
    typedef SOCKET socket_t;
    
    // Windows 特有的错误获取
    #define GET_SOCKET_ERROR() WSAGetLastError()
    
    // Windows 的 socket 操作
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SHUTDOWN_SEND SD_SEND
    
#else
    // Unix/Linux/macOS 平台
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <cstring>
    
    // Unix 使用 int 作为 socket 类型
    typedef int socket_t;
    
    // Unix 特有的错误获取
    #define GET_SOCKET_ERROR() errno
    
    // Unix 的 socket 操作
    #define CLOSE_SOCKET(s) close(s)
    #define SHUTDOWN_SEND SHUT_WR
    
    // 定义 Windows 特有的常量
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define SOCKET socket_t
    
    // WSA 相关类型和宏的占位定义
    struct WSADATA {
        unsigned short wVersion;
        unsigned short wHighVersion;
        char szDescription[257];
        char szSystemStatus[129];
        unsigned short iMaxSockets;
        unsigned short iMaxUdpDg;
        char* lpVendorInfo;
    };
    
    #define MAKEWORD(low, high) ((unsigned short)(((unsigned char)(low)) | ((unsigned short)((unsigned char)(high))) << 8))
    
    // WSA 函数的占位实现（仅用于编译兼容）
    inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
    inline void WSACleanup() {}
    inline int WSAGetLastError() { return errno; }
    
    // SOMAXCONN 在某些系统上可能未定义
    #ifndef SOMAXCONN
    #define SOMAXCONN 128
    #endif
#endif

// 跨平台的错误信息获取
inline const char* get_socket_error_string() {
#ifdef _WIN32
    static char buffer[256];
    int error = WSAGetLastError();
    std::snprintf(buffer, sizeof(buffer), "Error code: %d", error);
    return buffer;
#else
    return strerror(errno);
#endif
}

