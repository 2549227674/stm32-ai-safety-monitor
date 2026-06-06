# Task 02：WSL + i.MX6ULL SDK + SSH 部署链路

## 目标 / 范围

在 WSL 中配置 i.MX6ULL SDK，编译一个 hello 程序，通过 scp/ssh 推送到 i.MX6ULL 并运行。本任务不做 GPIO/I2C/PWM 业务。

## 前置条件

Task01 已完成。需要 i.MX6ULL 上电、联网、SSH 可用，且本地有 SDK。

> **[CLAUDE_CODE_TODO | VERIFY]** 恢复并复测 i.MX6ULL SSH / hello 板端运行
> - 为何 GPT 给不了：SDK 路径、CC、inventory 与 hello 交叉编译已由本地命令确认，但本轮 WSL/Windows 到 i.MX6ULL SSH 不可达。
> - 期望产物/操作：恢复板端网络后运行 `scripts/deploy_imx6ull.sh build/imx6ull/hello_imx6ull --run`。
> - 回填位置：Task02 前置条件和命令
> - 验收：板端输出 `hello from imx6ull target`。


## 操作步骤

1. 在 WSL 中定位 SDK 环境脚本或可用 `arm-buildroot-linux-gnueabihf-gcc`。（已完成：本 SDK 无 `environment-setup-*`，使用 `imx6ull.cc` fallback）
2. source SDK 或配置 `CC`。（已完成）
3. 新建 `hello.c`。（已完成）
4. 交叉编译。（已完成）
5. scp 到 i.MX6ULL。（本轮失败：SSH 不可达）
6. ssh 运行。（待复测）
7. 写部署脚本。（已完成：脚本从 `config/inventory.yaml` 读取）

## 命令骨架

```bash
scripts/build_imx6ull.sh
mkdir -p build/imx6ull edge/imx6ull-controller/src
file build/imx6ull/hello_imx6ull
scripts/deploy_imx6ull.sh build/imx6ull/hello_imx6ull --run
```

## 产出物

- `edge/imx6ull-controller/src/hello.c`
- `scripts/build_imx6ull.sh`
- `scripts/deploy_imx6ull.sh`
- `config/inventory.example.yaml`
- `tests/imx6ull/YYYY-MM-DD_toolchain_ssh.md`

## 验收标准

- [x] `$CC --version` 有真实输出。
- [ ] 目标文件能在板端运行。
- [x] 测试记录包含命令与输出。
- [x] 真实 IP/用户名未提交。

## 禁止事项

- 不把 SDK 路径写死到仓库脚本。
- 不提交 `inventory.yaml`。
- 不开始硬件控制。

## 完成后回填

- 更新 `AGENTS.md` i.MX SSH/SDK 状态。
- 更新 `DEVPLAN.md` P1。
- 回填 Task02 的 FILL TODO。
