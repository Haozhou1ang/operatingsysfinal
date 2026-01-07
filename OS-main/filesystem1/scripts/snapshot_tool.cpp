// scripts/snapshot_tool.cpp
#include "../include/disk.h"
#include "../include/inode.h"
#include <iostream>
#include <cstring>
using namespace std;

void print_usage(const char* prog_name) {
    cout << "用法: " << prog_name << " <command> [options]" << endl;
    cout << "命令:" << endl;
    cout << "  create <name>     创建快照" << endl;
    cout << "  delete <id>       删除快照" << endl;
    cout << "  list             列出所有快照" << endl;
    cout << "  restore <id>      恢复快照" << endl;
    cout << endl;
    cout << "示例:" << endl;
    cout << "  " << prog_name << " create my_backup" << endl;
    cout << "  " << prog_name << " list" << endl;
    cout << "  " << prog_name << " delete 0" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* disk_path = "../disk/disk.img";
    int fd = disk_open(disk_path);
    if (fd < 0) {
        cout << "无法打开磁盘文件: " << disk_path << endl;
        return 1;
    }
    
    const char* command = argv[1];
    
    if (strcmp(command, "create") == 0) {
        if (argc < 3) {
            cout << "错误: create命令需要指定快照名称" << endl;
            disk_close(fd);
            return 1;
        }
        
        int snapshot_id = create_snapshot(fd, argv[2]);
        if (snapshot_id >= 0) {
            cout << "快照创建成功，ID: " << snapshot_id << endl;
        } else {
            cout << "快照创建失败" << endl;
        }
    }
    else if (strcmp(command, "delete") == 0) {
        if (argc < 3) {
            cout << "错误: delete命令需要指定快照ID" << endl;
            disk_close(fd);
            return 1;
        }
        
        int snapshot_id = atoi(argv[2]);
        int result = delete_snapshot(fd, snapshot_id);
        if (result == 0) {
            cout << "快照删除成功" << endl;
        } else {
            cout << "快照删除失败" << endl;
        }
    }
    // 替换 scripts/snapshot_tool.cpp 中 list 命令的处理部分
else if (strcmp(command, "list") == 0) {
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(fd, snapshots, MAX_SNAPSHOTS);
    
    if (count < 0) {
        cout << "获取快照列表失败" << endl;
        disk_close(fd);
        return 1;
    }
    
    if (count == 0) {
        cout << "没有找到快照" << endl;
    } else {
        cout << "快照列表:" << endl;
        cout << "ID\t名称\t\t时间戳\t\t活动状态" << endl;
        cout << "----------------------------------------" << endl;
        
        for (int i = 0; i < count; i++) {
            cout << snapshots[i].id << "\t" << snapshots[i].name << "\t\t" 
                 << snapshots[i].timestamp << "\t\t" << (snapshots[i].active ? "是" : "否") << endl;
        }
    }
}
    else if (strcmp(command, "restore") == 0) {
        if (argc < 3) {
            cout << "错误: restore命令需要指定快照ID" << endl;
            disk_close(fd);
            return 1;
        }
        
        int snapshot_id = atoi(argv[2]);
        int result = restore_snapshot(fd, snapshot_id);
        if (result == 0) {
            cout << "快照恢复成功" << endl;
        } else {
            cout << "快照恢复失败" << endl;
        }
    }
    else {
        cout << "未知命令: " << command << endl;
        print_usage(argv[0]);
        disk_close(fd);
        return 1;
    }
    
    disk_close(fd);
    return 0;
}