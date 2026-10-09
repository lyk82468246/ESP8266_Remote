# ESP8266_Remote 开发入口

这是 Android_WIFI 的 ESP8266 下位机工程。先阅读 [README](README.md) 和 [.agents 目录说明](.agents/README.md)，再按任务阅读相关规则。用户在当前会话的指令优先于本目录约定。

## 按任务阅读

- 固件、引脚、外设或网络修改：[固件规则](.agents/rules/firmware.md)。
- API、HTTP、JSON 或状态一致性修改：[协议规则](.agents/rules/protocol.md)。
- 构建、测试或验收记录：[验证规则](.agents/rules/testing.md)。
- 实施、提交及推送：[开发流程](.agents/workflows/development.md)。

## 项目基线

- 默认板型 NodeMCU/ESP-12E，GPIO2（D4）板载 LED，低电平有效。用户同意以此开发，实物尚待核验。
- PlatformIO Core 6.1.18；espressif8266 4.2.1；Arduino Core 3.1.2；ArduinoJson 6.21.5。默认 AP 环境 `nodemcuv2`，STA 环境 `nodemcuv2_sta`。
- 协议来源：[Android_WIFI 协议 v1](https://github.com/zxu-source/Android_WIFI/blob/main/docs/protocol/wifi-control-v1.md)。查阅相关文档即可，不需克隆整个 Android 仓库。
- 本次工作只修改下位机。没有实际测试证据时，不宣称真板、无线链路或 Android 联调通过。
- `.agents/` 是可复用开发约定；测试结果放 `docs/testing/`，真实网络凭据放被忽略的本地配置。
