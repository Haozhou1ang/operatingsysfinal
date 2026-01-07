cmake_minimum_required(VERSION 3.10)
project(ReviewSystemClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 收集所有源文件
file(GLOB_RECURSE SOURCES 
    "src/network/*.cpp"
    "src/protocol/*.cpp"
    "src/session/*.cpp"
    "src/cli/*.cpp"
)

# 创建可执行文件
add_executable(client main.cpp ${SOURCES})

# Linux平台需要链接pthread
if(UNIX)
    target_link_libraries(client pthread)
endif()

# 设置编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(client PRIVATE -Wall -Wextra)
endif()

# 测试可执行文件（可选）
file(GLOB TEST_SOURCES "test/*.cpp")
if(TEST_SOURCES)
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file} ${SOURCES})
        if(UNIX)
            target_link_libraries(${test_name} pthread)
        endif()
    endforeach()
endif()

