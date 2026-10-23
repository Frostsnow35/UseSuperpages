# Findings

- 课程任务四要求 2 MiB 对齐区域使用 L1 超级页，并通过 `superpg_fork`/`superpg_free`。
- 基线失败：`superpg_fork failed: pte different`，原因是只有 L0 映射。
- 关键偏移：`walkaddr()` 必须加入 L1 叶子覆盖范围内的超级页偏移。
- 关键生命周期：部分释放要 demote；demote 后所有子页释放会留下空 L0 页表，重新分配前必须回收空表。
- 内存压力：分配超级页时预留 L0 页表页，确保 OOM 时仍可 demote。
- 固定池取舍：顶部 32 MiB、16 块；拆分块直到所有子页释放前不回收到普通池。
- 最终在 LAB=pgtbl 下 clean build、额外测试、官方 pgtbl 子测试和 usertests 均通过。
