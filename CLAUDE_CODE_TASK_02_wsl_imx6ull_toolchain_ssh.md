# Task 02：WSL + i.MX6ULL SDK + SSH 部署链路

## 目标 / 范围

在 WSL 中配置 i.MX6ULL SDK，编译一个 hello 程序，通过 scp/ssh 推送到 i.MX6ULL 并运行。本任务不做 GPIO/I2C/PWM 业务。

## 前置条件

Task01 已完成。需要 i.MX6ULL 上电、联网、SSH 可用，且本地有 SDK。

> **[CLAUDE_CODE_TODO | FILL]** 填写 SDK 路径与 i.MX6ULL SSH 信息
> - 为何 GPT 给不了：这些值只能在本地环境确认。
> - 期望产物/操作：填写 `WSL_SDK_ENV=<TODO:FILL>`、`IMX_USER=<TODO:FILL>`、`IMX_IP=<TODO:FILL>`、`IMX_DEPLOY_DIR=<TODO:FILL>`。
> - 回填位置：Task02 前置条件和命令
> - 验收：能 source SDK、SSH 登录板端。


## 操作步骤

1. 在 WSL 中定位 SDK 环境脚本。
2. source SDK。
3. 新建 `hello.c`。
4. 交叉编译。
5. scp 到 i.MX6ULL。
6. ssh 运行。
7. 写部署脚本。

## 命令骨架

```bash
source <WSL_SDK_ENV_TODO_FILL>
$CC --version
mkdir -p build/imx6ull edge/imx6ull-controller/src
cat > edge/imx6ull-controller/src/hello.c <<'EOF'
#include <stdio.h>
int main(void) { puts("hello from imx6ull target"); return 0; }
EOF
$CC edge/imx6ull-controller/src/hello.c -o build/imx6ull/hello_imx6ull
scp build/imx6ull/hello_imx6ull <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL>:<IMX_DEPLOY_DIR_TODO_FILL>/
ssh <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL> <IMX_DEPLOY_DIR_TODO_FILL>/hello_imx6ull
```

## 产出物

- `edge/imx6ull-controller/src/hello.c`
- `scripts/build_imx6ull.sh`
- `scripts/deploy_imx6ull.sh`
- `config/inventory.example.yaml`
- `tests/imx6ull/YYYY-MM-DD_toolchain_ssh.md`

## 验收标准

- [ ] `$CC --version` 有真实输出。
- [ ] 目标文件能在板端运行。
- [ ] 测试记录包含命令与输出。
- [ ] 真实 IP/用户名未提交。

## 禁止事项

- 不把 SDK 路径写死到仓库脚本。
- 不提交 `inventory.yaml`。
- 不开始硬件控制。

## 完成后回填

- 更新 `AGENTS.md` i.MX SSH/SDK 状态。
- 更新 `DEVPLAN.md` P1。
- 回填 Task02 的 FILL TODO。
