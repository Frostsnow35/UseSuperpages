# Use superpages 任务计划

- [x] 阅读课程 PDF 第 3.5 节和 MIT 2025 pgtbl 页面
- [x] 基线运行并确认 `pte different`
- [x] 实现独立超级页池、L1 映射、fork 复制和释放
- [x] 实现部分释放 demotion、空 L0 回收和 OOM 预留页表
- [x] 补齐 USYSCALL/ugetpid 与 vmprint
- [x] 增加额外生命周期回归测试
- [x] 在虚拟机中 clean build、官方测试、usertests 和额外测试
- [x] 整理过程文档、验证证据与源码哈希
- [x] 提交并推送 GitHub，核对远端提交

当前实现面向 `conf/lab.mk: LAB=pgtbl`。
