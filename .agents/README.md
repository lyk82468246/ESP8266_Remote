# 开发约定目录

根目录 `AGENTS.md` 是入口；本目录按职责拆分，避免每次任务都加载全部细节。

```text
.agents/
  README.md                  目录职责和维护原则
  rules/
    firmware.md              板级配置、驱动、网络和内存约束
    protocol.md              HTTP/JSON 契约与状态一致性
    testing.md               按修改范围选择验证与记录证据
  workflows/
    development.md           从检查工程到提交推送的流程
```

固件入口在 `ESP8266_Remote/ESP8266_Remote.ino`；`Config.h` 管理板级注册和容量，`WiFiConfig.h` 保存项目网络参数，`Resources.h` 负责资源注册、全局 revision 和采样调度，`Peripheral.h` 实现二态/PWM/ADC 驱动，`Validation.h` 校验标识和显示名，`NetworkService.h` 管理网络与 mDNS，`HttpRequest.h` / `StrictJson.h` / `Api.h` 管理协议。测试与工具分别在 `tests/`、`tools/`，接线在 `docs/hardware/`，构建配置在 `platformio.ini`。

维护原则：只写影响本项目行为的约定；命令、路径和版本必须对应实际工程。功能或工具入口变化时同步相关规则和 README。不要复制所有上位机文档，不在此保存会话日志、待办流水、私密设置或未经验证的验收结论。新增规则需明确职责和适用任务，并更新根入口。
