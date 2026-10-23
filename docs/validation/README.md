# 验证证据

以下文件来自 Linux 虚拟机中实际运行，未经修改测试断言。部分 `make` 中文状态信息使用虚拟机原始 locale 编码。

| 文件 | 内容 |
| --- | --- |
| `baseline.log` | 原实验目录尚无超级页时的 `pte different` 失败 |
| `extra-before-fix.log` | 首次扩展测试发现空 L0 复用 panic |
| `build.log` | `make clean && make -j4` 输出，退出 0 |
| `official.log` | 官方 ugetpid、print_kpgtbl、superpg 和 usertests 检查，退出 0 |
| `usertests.out` | usertests 原始输出，包含 `ALL TESTS PASSED` |
| `extra.log` | 加入严格 OOM demotion 后额外 runner 通过，退出 0 |
| `extra-and-pgtbltest.out` | 额外程序及完整官方 pgtbltest 的原始输出 |
| `final-header-build.log` | PTE_LEAF 宏修正后的编译输出，退出 0 |
| `final-header-extra.log` | 宏修正后的额外 runner，退出 0 |
| `final-extra-and-pgtbltest.out` | 最终源码运行额外程序和官方 pgtbltest 的原始输出 |
| `source-sha256.txt` | 在虚拟机中计算的 11 个源码/构建输入哈希，已核对本地一致 |
| `source-sha256-lf.txt` | 同一批输入按 Git 提交的 LF 换行计算的哈希，便于 Linux clone 后 `sha256sum -c` |
| `environment.txt` | 编译器、QEMU、Python、make 版本 |

最后一次完整 usertests 在宏修正前运行；修正仅去掉 PTE_LEAF 定义外的条件，在 LAB=pgtbl 配置中的预处理内容相同。之后重新编译并通过额外测试及完整 pgtbltest。

官方 runner 的 usertests 项仅等待 shell 命令完成，因此还人工核对了 `usertests.out` 的 `ALL TESTS PASSED` 标志。未将个人 answers/time 项计入验收。

Windows 本地工作副本和传入虚拟机的源文件多数使用 CRLF，Git 提交会规范成 LF。两个 manifest 分别记录实际测试输入字节和仅规范换行后的提交内容；差异只来自换行，不影响 C/Python 语义。`.gitattributes` 保留原始验证输出及其末尾空格，避免把日志本身改写成代码风格。
