# 验证与证据规则

按实际修改选择验证，不为纯文档或忽略规则改动重复全量编译。

| 修改范围 | 必要验证 |
| --- | --- |
| 文档、`.agents/`、`.gitignore` | 核对命令/路径/引用，检查忽略样例与待提交列表，`git diff --check` |
| API、HTTP、JSON、资源状态或驱动 | `./tools/test_native.ps1`，以及受影响固件环境的构建 |
| 通用固件代码或构建配置 | AP 与 STA 环境构建；行为变化时执行契约测试 |
| Python 联调工具 | `python -m py_compile tools/check_device.py` 与 `--help`；网络行为变化按条件联调 |
| GPIO、供电、网络接入或实物行为 | 编译与相关契约测试；具备设备时补实物验证，未执行则明确记录 |

当前 Windows 工作区入口：

```powershell
./tools/pio.ps1 run -e nodemcuv2
./tools/pio.ps1 run -e nodemcuv2_ap
./tools/test_native.ps1
python -m py_compile tools/check_device.py
python tools/check_device.py --help
```

本机契约测试需要 ArduinoJson 已由 `nodemcuv2` 构建安装，并需要 C++ 编译器；测试入口默认使用工程 `.pio-core` 下的 MinGW。新机器安装方法见 README，可通过 `-Compiler` 指定其他 g++ 路径。

`nodemcuv2` 现在是默认 STA 构建；`nodemcuv2_sta` 为显式 STA 的兼容入口。测试所需 ArduinoJson 路径来自 `nodemcuv2` 构建，不能只构建 AP 后假定存在该路径。本机测试含独立网络替身，覆盖 mDNS 及重连；它不证明真实组播广播成功。Schema 对照使用 README 中固定上位机提交的一份 Schema 与实际 API 输出，不复制模拟设备的外设清单。

`tools/check_device.py` 是真实控制脚本，会开关输出；仅在当前任务已包含实物联调且确认目标资源时运行。测试默认 10 轮、20 次状态变化，还检查重复写与拒绝请求；人工观察外设，不以返回 200 替代实物变化证据。

结果写入 `docs/testing/validation.md` 或有明确日期的记录，区分通过、失败、未执行与受阻。记录环境、命令、固件容量和验证范围，不将本机替身通过等同于真板通过。原始私密抓包/日志保存在忽略的 `docs/testing/evidence/local/`；审查并脱敏后的证据可提交到 evidence 的其他路径。
