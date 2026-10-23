# 工作进度

截至 2026-10-23：

- 已完成 `kernel/kalloc.c`、`kernel/vm.c`、`kernel/defs.h`、`kernel/proc.c`、`kernel/proc.h` 和 `kernel/riscv.h` 的实现。
- 已加入 `user/superpgextra.c` 与 `grade-superpages-extra`。
- 已修复重复地址复用时空 L0 页表导致的 `supermappages: remap`。
- 已验证 clean build、官方 `ugetpid`/`print_kpgtbl`/`superpg`、`usertests -q` 和额外 OOM demotion 测试。
- 已提交实现 `38be58f1a4fe1ee3a6b8865e1cbf8786f2a0f296` 并推送目标 GitHub 仓库 `main`；`git ls-remote` 核对远端一致。
- 已修复上传后本地引用/跟踪配置因沙箱权限未能写入的问题，`main` 正确跟踪 `origin/main`。
- 最终核对：11 个已提交构建/测试输入的 SHA-256 与测试源码 LF 清单一致；官方测试文件与基线一致。
- 实现、测试、过程文档和上传工作均已完成。
