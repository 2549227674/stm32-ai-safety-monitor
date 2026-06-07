# 2026-06-07 Task07-C2 init.d 版 imx_safetyd 测试记录

## 1. 分支与提交

- 分支：migration/imx6ull-opi5-edge-ai
- 当前阶段：Task07-C2 init.d 启动管理
- 基线：de8c180 task07-c: add c imx safetyd daemon

## 2. 结论

- init.d 脚本新增：通过
- 安装到板端：通过
- env 文件：通过（板端本地创建，未入库）
- start：通过
- status：通过
- stop：通过
- restart：通过
- 日志：通过
- status JSON：通过
- 开机自启：已确认条件具备，未重启实测
- systemd 不支持已记录：通过
- 未提交 env/inventory/图片/数据库/spool：确认

## 3. 为什么 systemd 不可用

板端执行 `systemctl` 返回 `systemctl not available`，说明当前 i.MX6ULL 镜像不含 systemd 管理栈。本轮不尝试安装或修复 systemd。

## 4. 安装路径

- init.d 脚本：`/etc/init.d/S99imx-safetyd`
- 安装来源：`edge/imx6ull-controller/init.d/S99imx-safetyd`
- 安装方式：`scripts/install_imx_safetyd_initd.sh`

## 5. env 文件路径

- env 文件：`/etc/edge-ai-safety-monitor/imx-safetyd.env`
- 内容字段：`OPI5_URL`、`BACKEND_URL`、`DEVICE_ID`、`GPIO_PIR`、`GPIO_ACTIVE_HIGH`、`CAPTURE_DEVICE`、`BASE_DIR`、`POST_NORMAL`、`INTERVAL_SEC`
- 说明：env 文件在板端本地创建，不提交到仓库，不写真实密码到文档

## 6. 安装结果

安装命令：

```bash
scripts/install_imx_safetyd_initd.sh
```

输出摘要：

```text
[install] 已安装 /etc/init.d/S99imx-safetyd
[install] 若板端尚无 /etc/edge-ai-safety-monitor/imx-safetyd.env，请手动创建；本脚本不会上传真实配置
```

板端检查：

```text
-rwx------ 1 root root 3860 ... /etc/init.d/S99imx-safetyd
ls: cannot access '/etc/edge-ai-safety-monitor/imx-safetyd.env': No such file or directory
```

随后在板端本地创建了 `/etc/edge-ai-safety-monitor/imx-safetyd.env`，仅用于本轮验证，不入库。

## 7. start / status

命令：

```bash
/etc/init.d/S99imx-safetyd start
/etc/init.d/S99imx-safetyd start
/etc/init.d/S99imx-safetyd status
ps | grep imx_safetyd | grep -v grep
```

输出摘要：

```text
[imx-safetyd] started (pid 981)
[imx-safetyd] already running (pid 981)
[imx-safetyd] running (pid 981)
981 root /opt/edge-ai-safety-monitor/imx_safetyd --mode loop ...
```

日志与 status：

```text
/opt/edge-ai-safety-monitor/run/imx_safetyd.log
/opt/edge-ai-safety-monitor/run/imx_safetyd_status.json
```

日志摘要：

```text
[start] mode=loop device_id=labbox_001 base_dir=/opt/edge-ai-safety-monitor
[loop] start interval_sec=2
[gpio] gpio117 raw=0 pir=0 active_high=1 force_verify=0
[state] NORMAL risk_before_ai=0 need_snap=false
[skip] NORMAL and POST_NORMAL=0
```

status JSON 摘要：

```json
{
  "program": "imx_safetyd_c",
  "last_seq": 0,
  "last_state": "NORMAL",
  "last_pir": 0,
  "last_risk_score": 0,
  "last_ai_status": "skipped",
  "last_backend_status": "skipped",
  "last_error": null
}
```

## 8. stop / status

命令：

```bash
/etc/init.d/S99imx-safetyd stop
/etc/init.d/S99imx-safetyd status
ps | grep imx_safetyd | grep -v grep
```

输出摘要：

```text
[imx-safetyd] stopping pid 981
[imx-safetyd] stopped
[imx-safetyd] not running
```

结论：进程停止，PID 文件删除，status 能显示 stopped。

## 9. restart

命令：

```bash
/etc/init.d/S99imx-safetyd stop
/etc/init.d/S99imx-safetyd restart
/etc/init.d/S99imx-safetyd status
ps | grep imx_safetyd | grep -v grep
```

输出摘要：

```text
[imx-safetyd] not running
[imx-safetyd] not running
[imx-safetyd] started (pid 1045)
[imx-safetyd] running (pid 1045)
1045 root /opt/edge-ai-safety-monitor/imx_safetyd --mode loop ...
```

说明：修复后 `restart` 在 stopped 状态也会继续执行 start，行为正常。最后停止态 `status` 返回 3，属于 init.d stopped 状态语义。

## 10. 开机自启判断

板端 `/etc/inittab` 包含：

```text
::sysinit:/etc/init.d/rcS
```

板端 `/etc/init.d/rcS` 包含：

```text
for i in /etc/init.d/S??* ;do
```

因此当前 BusyBox / SysV 镜像具备开机自启条件，但本轮没有重启板子做最终实测。

## 11. 边界

- 当前采用 BusyBox/SysV init.d，不是 systemd。
- systemd service 模板保留备用，不删除。
- C 程序仍使用 OPi5 mock AI。
- 不接 MOS/水泵/220V。
- 不提交 env、inventory、图片、数据库、spool、uploads。
- 本轮不把手动 start 写成已开机自启，除非真的重启验证。

## 12. 下一步

- Task09：最终演示脚本与证据包整理。
- Task05-B：RKNN demo 验证。
- Task07-D：若后续镜像更新，再评估 systemd 或更完整守护化方案。
