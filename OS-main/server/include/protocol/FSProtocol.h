#pragma once
#include <string>
#include <vector>

// 【关键】标注所有需 FileSystem 提供的 API
// 注意：此头文件不包含任何 FS 实现，仅定义接口
class FSProtocol {
public:
    virtual ~FSProtocol() = default;

    // 【FileSystem API 调用点 1】创建快照
    virtual bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 2】恢复快照
    virtual bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 3】列出快照
    virtual std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 4】读取文件
    virtual bool readFile(const std::string& path, std::string& content, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 5】写入文件
    virtual bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 6】删除文件
    virtual bool deleteFile(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 7】创建目录
    virtual bool createDirectory(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 8】获取文件权限
    virtual std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 9】提交审核请求
    virtual std::string submitForReview(const std::string& operation, const std::string& path, 
                                       const std::string& user, std::string& errorMsg) = 0;

    // 【新增】目录遍历/类型查询（用于 CLI: LS/TREE）
    // entries 返回“当前目录下的直接子项名字”，目录项建议以 '/' 结尾表示目录。
    virtual bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) = 0;
    // 判断 path 是否为目录；若 path 不存在或读取失败，返回 false 并填写 errorMsg。
    virtual bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) = 0;
};