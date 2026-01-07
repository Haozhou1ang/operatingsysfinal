# 修改说明

本次更新围绕“引用计数持久化 + 一致性检查增强”进行，目标是保证快照/COW 在重启后仍一致，并补足更完整的 fsck 逻辑。

## 1. 引用计数持久化（解决跨重启丢失）
- **Superblock 扩展**：新增 `refcount_table_start` 与 `refcount_table_blocks` 字段，用于记录引用计数表在磁盘中的位置与长度。
- **布局计算**：在 `Superblock::init` 中加入引用计数表块数的迭代计算，保证元数据与数据区边界稳定。
- **mkfs 初始化**：格式化时写入引用计数表，并将根目录数据块的引用计数设为 1。

## 2. Allocator 增强
- **读写引用计数表**：新增 `loadRefcountTable()` / `saveRefcountTable()`，并在 `sync()` 时根据 `refcount_dirty_` 写回。
- **引用计数变更跟踪**：`allocBlock` / `freeBlock` / `incBlockRef` / `decBlockRef` 都会标记 `refcount_dirty_`。
- **一致性检查扩展**：
  - 继续校验 inode/block 计数与 root inode 位图。
  - 新增对 refcount 与 block bitmap 是否匹配的检查，并在 `fix=true` 时修正。
  - 新增 `reconcileUsage()`，可用“真实使用集合”对位图与计数进行校正。

## 3. Snapshot/FS 一致性与遍历
- **使用情况收集**：新增 `collectUsage()` / `collectForInode()`，遍历根目录与快照根，收集实际使用的 inode 与 block 集合。
- **挂载/快照操作后校验**：
  - 挂载、快照恢复后确保 refcount 重建。
  - 创建/删除快照后调用一致性检查，必要时触发重建。

## 4. 间接块统计策略
- 引入 `count_indirect_blocks_` 开关，用于控制是否将间接块计入引用计数，避免某些场景下统计重复。

---

如需后续扩展：可将 `rebuildBlockRefcounts()` 作为异常修复路径，同时主路径优先使用持久化 refcount，以降低挂载开销。
