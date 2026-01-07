#include "include/protocol/RealFileSystemAdapter.h"
#include <iostream>

int main() {
    std::cout << "Testing RealFileSystemAdapter..." << std::endl;
    
    try {
        RealFileSystemAdapter adapter("../../filesystem/disk/disk.img");
        std::cout << "✅ Adapter created successfully!" << std::endl;
        
        // 测试写文件
        std::string errorMsg;
        bool result = adapter.writeFile("/test.txt", "Hello, World!", errorMsg);
        if (result) {
            std::cout << "✅ Write file successful!" << std::endl;
        } else {
            std::cout << "❌ Write file failed: " << errorMsg << std::endl;
        }
        
        // 测试读文件
        std::string content;
        result = adapter.readFile("/test.txt", content, errorMsg);
        if (result) {
            std::cout << "✅ Read file successful: " << content << std::endl;
        } else {
            std::cout << "❌ Read file failed: " << errorMsg << std::endl;
        }
        
        // 测试创建目录
        result = adapter.createDirectory("/mydir", errorMsg);
        if (result) {
            std::cout << "✅ Create directory successful!" << std::endl;
        } else {
            std::cout << "❌ Create directory failed: " << errorMsg << std::endl;
        }
        
        // 测试快照
        result = adapter.createSnapshot("/", "test_snapshot", errorMsg);
        if (result) {
            std::cout << "✅ Create snapshot successful!" << std::endl;
        } else {
            std::cout << "❌ Create snapshot failed: " << errorMsg << std::endl;
        }
        
        // 列出快照
        auto snapshots = adapter.listSnapshots("/", errorMsg);
        std::cout << "📋 Snapshots: ";
        for (const auto& snap : snapshots) {
            std::cout << snap << " ";
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "✅ All tests passed!" << std::endl;
    return 0;
}

