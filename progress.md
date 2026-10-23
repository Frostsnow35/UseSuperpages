# 工作进度

截至 2026-10-23：

- 已完成 `kernel/kalloc.c`、`kernel/vm.c`、`kernel/defs.h`、`kernel/proc.c`、`kernel/proc.h` 和 `kernel/riscv.h` 的实现。
- 已加入 `user/superpgextra.c` 与 `grade-superpages-extra`。
- 已修复重复地址复用时空 L0 页表导致的 `supermappages: remap`。
- 已验证 clean build、官方 `ugetpid`/`print_kpgtbl`/`superpg`、`usertests -q` 和额外 OOM demotion 测试。
- 待完成：提交本地变更、设置目标 GitHub origin、push 并用 `git ls-remote` 核对远端 commit。
