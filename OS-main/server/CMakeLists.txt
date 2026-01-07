cmake_minimum_required(VERSION 3.10)
project(AcademicPaperServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 src 目录下的所有 .cpp 源文件
file(GLOB_RECURSE CPP_FILES "src/*.cpp")

# 是否使用真实 filesystem 模块（Windows/MSVC 下默认关闭，因为 filesystem 源码依赖 unistd.h）
option(USE_REAL_FILESYSTEM "Build with real filesystem module" ON)
if(MSVC)
    set(USE_REAL_FILESYSTEM OFF)
endif()

# 添加 filesystem 模块的源文件
set(FS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../filesystem")
set(FS_SOURCES
    "${FS_DIR}/src/disk.cpp"
    "${FS_DIR}/src/inode.cpp"
    "${FS_DIR}/src/directory.cpp"
    "${FS_DIR}/src/path.cpp"
    "${FS_DIR}/src/block_cache.cpp"
)

if(NOT USE_REAL_FILESYSTEM)
    # 不构建真实 filesystem 时，避免编译 RealFileSystemAdapter.cpp（否则会链接到 disk_open 等符号）
    list(REMOVE_ITEM CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/RealFileSystemAdapter.cpp")
endif()

# 将 main.cpp、server 源文件以及（可选的）filesystem 源文件共同作为服务器的源文件
if(USE_REAL_FILESYSTEM)
    set(SERVER_SOURCES main.cpp ${CPP_FILES} ${FS_SOURCES})
else()
    set(SERVER_SOURCES main.cpp ${CPP_FILES})
endif()

# 定义服务器可执行文件
add_executable(server ${SERVER_SOURCES})

if(MSVC)
    # 解决中文源码在 MSVC 下的编码解析问题
    target_compile_options(server PRIVATE /utf-8)
endif()

if(USE_REAL_FILESYSTEM)
    target_compile_definitions(server PRIVATE SERVER_USE_REAL_FS=1)
else()
    target_compile_definitions(server PRIVATE SERVER_USE_REAL_FS=0)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_client.cpp")
    # 定义测试客户端可执行文件（如果存在）
    add_executable(test_client test/test_client.cpp)
endif()

# 为服务器和客户端设置 include 目录
# ${CMAKE_CURRENT_SOURCE_DIR} 指向 server 目录，可以确保 include 目录被正确找到
target_include_directories(server PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(USE_REAL_FILESYSTEM)
    target_include_directories(server PUBLIC ${FS_DIR}/include)
endif()
if(TARGET test_client)
    target_include_directories(test_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# 为 Windows 平台链接 Winsock 库
if(WIN32)
    target_link_libraries(server ws2_32)
    if(TARGET test_client)
        target_link_libraries(test_client ws2_32)
    endif()
endif()

# 打印出可执行文件的生成位置，方便您查找
message(STATUS "Server executable: $<TARGET_FILE:server>")
if(TARGET test_client)
    message(STATUS "Client executable: $<TARGET_FILE:test_client>")
endif()