# xv6 Use superpages（MIT 2025）

基于 xv6-labs-2025 `pgtbl` 分支，实现课程 Lab3 任务四：让合适的 `sbrk()` 新增范围使用 **2 MiB L1 超级页**，支持 fork、整体释放和部分释放 demotion。补齐 USYSCALL/ugetpid 和 vmprint，以运行完整官方 `pgtbltest`。

## 验证结果

2026-10-23 在本机 Linux 虚拟机内以 RISC-V GCC / QEMU 验证：

| 检查 | 结果 |
| --- | --- |
| `make clean` 后 `make -j4` | 成功，退出码 0 |
| 官方 `superpg_fork` / `superpg_free` | 均为 `OK` |
| 完整 `pgtbltest` | `all tests succeeded` |
| `ugetpid` / `print_kpgtbl` 官方检查 | 均为 `OK` |
| `usertests -q` | `ALL TESTS PASSED` |
| 额外生命周期与内存压力测试 | `all tests succeeded` |

官方 `user/pgtbltest.c`、`grade-lab-pgtbl` 和 `gradelib.py` 保持基线内容。这里列出的是代码运行检查，未填写个人 `time.txt`，也未代写 `answers-pgtbl.txt`，因此不将此结果描述为整套 `make grade` 满分。

## 阅读路线

- [实现思路与完整过程](UseSuperpages-实现思路与过程.md)：页表层级、分配器、fork、demotion、失败回滚和设计取舍。
- [过程记录](过程记录.md)：踩坑、根因、修复和验证过程。
- [验证证据](docs/validation/README.md)：构建、官方输出、额外测试、失败样本和源码 SHA-256。
- 核心代码：[kalloc.c](kernel/kalloc.c)、[vm.c](kernel/vm.c)、[proc.c](kernel/proc.c)。
- 扩展测试：[superpgextra.c](user/superpgextra.c)、[grade-superpages-extra](grade-superpages-extra)。

## 编译与复现

依赖 `make`、RISC-V GCC/binutils、`qemu-system-riscv64` 和 Python 3。保持 `conf/lab.mk` 中 `LAB=pgtbl`：

```sh
make clean
make -j4
python3 -W ignore -u grade-superpages-extra
python3 -W ignore -u grade-lab-pgtbl ugetpid print_kpgtbl superpg usertests
```

也可 `make qemu` 后在 xv6 shell 分别执行 `pgtbltest`、`superpgextra`、`usertests -q`，退出 QEMU 按 `Ctrl-a` 后按 `x`。官方用例故意访问已释放地址时打印的 page fault 属于预期行为。

## 实现范围与取舍

普通页池和 16 个 2 MiB 物理块组成的超级页池分离；头尾使用 4 KiB 页，完整对齐区间优先超级页，超级池耗尽时申请内存回退普通页。部分释放原地拆成 512 个 L0 映射；拆分块直到最后一个子页释放后才整块回收。每个超级页预留一个 4 KiB 拆分页表，支持普通内存耗尽时 demotion，代价是没有净省下对应的 L0 页表内存。fork 若超级池不足会失败并清理，不实现动态 promotion/compaction。

参考：[MIT 2025 pgtbl lab](https://pdos.csail.mit.edu/6.828/2025/labs/pgtbl.html)。上游 xv6 的介绍、作者和构建说明保存在原始 [README](README)，许可见 [LICENSE](LICENSE)。
