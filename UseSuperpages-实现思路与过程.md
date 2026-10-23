# Use superpages：实现思路与完整过程

## 目标模型

RISC-V Sv39 页表从 L2 到 L1 到 L0 查找。普通页在 L0 PTE 结束；2 MiB 超级页在 L1 PTE 直接结束。L1 叶子必须同时满足：PTE 有效、R/W/X 至少有一个权限位、物理地址按 2 MiB 对齐。这样 512 个连续的 4 KiB 虚拟页共享一个 PTE，减少页表占用和 TLB 压力。

本实现选择“申请时直接分配超级页”：`uvmalloc(oldsz, newsz)` 从向上取整后的旧地址开始前进。当当前位置 2 MiB 对齐且剩余长度至少 2 MiB 时尝试 `superalloc()` 并安装一个 L1 叶子；否则使用原来的 4 KiB `kalloc()`/`mappages()` 路径。超级页池耗尽时回退到普通页，保证普通内存仍可使用。

## 物理内存分配器

`kernel/kalloc.c` 新增独立的 `supermem` 锁、空闲链表和 16 个块的元数据。每块记录：

- `refs = -1`：完整超级页正在使用；
- `refs = 0`：完整超级页空闲；
- `refs = 1..512`：已经 demote，仍存活的 4 KiB 子页数；
- `split`：为未来 demotion 预留的 L0 页表页。

初始化时普通 `freerange()` 只释放到 `SUPERBASE`，顶部 32 MiB 逐块加入超级页链表。`superalloc()` 取出一块并预留一个普通页作为 L0 页表；预留失败会回滚整块。`superfree()` 释放完整块及预留页表。`superdemote()` 把预留页表转给页表代码并把计数设为 512。子页的 `kfree()` 不进入普通链表，而是递减对应块计数，最后一页释放时整块回收。

## 页表遍历和物理地址

`walklevel()` 保持 `walk()` 的接口兼容，同时返回叶子所在层级。遍历从 L2 开始：目标层到达时返回；途中遇到 R/W/X 非零的有效 PTE 时，说明已遇到叶子，立即返回。这同时覆盖 L1 超级叶子和普通 L0 叶子。

`walkaddr()` 根据叶子层级添加覆盖范围内的偏移：L0 添加 0，L1 添加虚拟地址在 2 MiB 超级页内的 4 KiB 页偏移；调用者再加 4 KiB 页内偏移。这个顺序让 `copyin`、`copyout`、`copyinstr` 和用户态直接访存使用同一套地址语义。

## 分配、复制和释放

### uvmalloc

每次循环只安装一个叶子。映射失败时按分配类型释放当前物理块，并调用 `uvmdealloc()` 撤销此前本次增长，避免留下半截地址空间。

### uvmcopy

fork 遍历父页表中的实际叶子跨度：L1 叶子分配新超级块并复制 2 MiB 数据，L0 叶子按原路径复制 4 KiB。新页表安装失败会释放当前块并撤销已经复制的区间。父子进程不共享可写超级物理页，满足官方 fork 检查的数据隔离要求。

### uvmunmap 和 demote

完整覆盖一个 L1 叶子时直接清除 PTE 并调用 `superfree()`。如果释放从超级页中间开始、结束于超级页内部，或范围不足一整块，则调用 `demote()`：填充 512 个连续 L0 PTE，替换 L1 non-leaf，刷新 TLB，再从原地址重试。释放后的子页由 `kfree()` 计数回收。`freewalk()` 继续要求所有叶子先由 `uvmunmap()` 删除，之后只递归释放空的页表页。

### 任务二 USYSCALL 和任务三 vmprint

`proc.c/proc.h` 增加每进程 USYSCALL 页：创建进程时分配并写入 pid，页表以 `PTE_R|PTE_U` 映射，销毁时先撤销映射再释放物理页。`vmprint()` 递归打印有效 PTE，按 L2/L1/L0 的 `PXSHIFT(level)` 拼接虚拟地址，遇到叶子停止递归。

## 额外回归测试设计

`user/superpgextra.c` 覆盖：

1. 连续 2 MiB 区域反复分配和释放；
2. 超级页非零偏移的 pipe `copyin/copyout` 和跨页 `copyinstr`；
3. 部分释放后的 demotion、数据保留和 fork 隔离；
4. 超级池耗尽后的普通页回退、增长失败回滚；
5. 普通页耗尽时使用预留页表完成 demotion。

`grade-superpages-extra` 先运行额外程序，再运行官方未修改的 `pgtbltest`，所以能在一次启动中同时检查扩展生命周期和官方行为。

## 关键时间线

- 阅读课程 PDF 第 3.5 节和 MIT 页面，确认硬件规则、对齐条件与测试要求。
- 运行基线：官方 `superpg_fork` 报 `pte different`。
- 实现分池、L1 映射、fork 复制、部分释放 demotion、USYSCALL 和 vmprint。
- 官方测试通过后增加生命周期测试。
- 生命周期测试发现重复地址复用时的 `supermappages: remap`；追踪发现空 L0 页表没有在最后一个子页释放时从树上摘除。
- 在 `supermappages` 中验证 non-leaf 为空后回收，再安装新的 L1 叶子。
- 增加 OOM demotion 场景；最终全部回归通过。

## 复现步骤

在带有 RISC-V GCC、QEMU 和 Python 的环境中：

```sh
git clone https://github.com/Frostsnow35/UseSuperpages.git
cd UseSuperpages
# conf/lab.mk 应为 LAB=pgtbl
make clean
make -j4
python3 -W ignore -u grade-superpages-extra
python3 -W ignore -u grade-lab-pgtbl ugetpid print_kpgtbl superpg usertests
```

预期结果包括：`superpg_fork: OK`、`superpg_free: OK`、`pgtbltest: all tests succeeded`、`superpgextra: all tests succeeded` 和 `ALL TESTS PASSED`。

## 设计边界

本版本是教学实现，不做运行时 promotion，也不做物理内存 compaction。固定 16 块超级页容易理解并满足课程测试，但 demote 后的块会保留到所有 512 个子页释放。`fork()` 为完整超级页申请子进程的超级块，固定池不够时返回失败并回滚，即使普通页池还可能有空闲；这属于资源不足的失败路径。

为每个完整超级页预留一个 4 KiB 拆分页表，保证了内存耗尽时仍可缩小堆，但也抵消了这一映射原本可以省下的 L0 页表页内存。本实现真正建立了 L1 叶子并减少硬件页表遍历层数；未测量性能提升，也不声称已经实现页表内存净节省。

普通池耗尽、超级池耗尽和拆分块占用是三种不同资源状态。`refs` 记录的是块内仍被映射的物理子页数量，不是多进程共享页的引用计数。

真正的生产系统还要处理碎片整理、动态提升、并发回收和更丰富的页大小策略。课件用数据库历史上的 THP 部署取舍说明：大页减少 TLB 开销，而同步整理或分配大块物理页可能带来尾延迟。本实现预留物理块与拆分页表，避免了运行中整理物理内存的复杂度，代价是固定池利用率。
