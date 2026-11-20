# 文档索引

本目录按“当前说明”和“历史证据”分开维护。当前行为以源码、根目录 README 和本页列出的最新记录为准；带日期的旧记录保留当时的验证事实，不代表当前固件版本。

## 当前说明

| 文档 | 内容 |
| --- | --- |
| [根 README](../README.md) | 构建、运行、API、协议边界和开发入口 |
| [外设接线与控制](hardware/peripherals.md) | NodeMCU 引脚表、LED/PWM、A0、电位器、D7 微波干接点、电机边界 |
| [TLC549 光敏通道](hardware/tlc549.md) | 暂缓启用的实验接线；不属于当前能力清单 |
| [开发约定](../.agents/README.md) | 固件、协议、验证和提交推送规则 |

当前固件基线为 0.9.0：D7/GPIO13 使用 GPIO 中断，STA 地址使用 WiFi 事件标志，A0/PWM 联动周期为 50 ms（20 Hz），项目 WiFi 配置保存在受版本管理的 `ESP8266_Remote/WiFiConfig.h`。

## 最新验证

- [D7 GPIO 中断与 20 Hz 采样验证（2026-11-20）](testing/2026-11-20-interrupt-sampling.md)：当前源码、STA/AP 构建、契约测试、COM6 烧录和串口启动证据。

该记录确认了固件启动并取得 STA 地址，但没有把未执行的微波边沿、电位器变化或 Android 真机联调写成通过。

## 历史记录

以下记录用于追溯演进过程，版本、容量和采样周期以记录当日为准：

- [微波输入初次接入（0.7.0）](testing/2026-11-20-microwave-input.md)；当前实现细节以 0.9.0 记录为准。
- [电位器与红绿 PWM 联动初次实现（0.8.0）](testing/2026-11-20-pot-pwm-linkage.md)；当前采样周期已调整为 50 ms。
- [TLC549 实验验证](testing/2026-11-13-tlc549.md)；当前 TLC549 仍由编译期开关屏蔽。
- [清单适配、ADC 电压和 PWM JSON 历史记录](testing/2026-10-16-capability-adaptation.md) / [pot-voltage](testing/2026-10-16-pot-voltage.md) / [pwm-json](testing/2026-10-16-pwm-json.md)。
- [早期综合验证记录](testing/2026-10-16-peripherals.md) 与 [validation](testing/validation.md)。

修订当前行为时同步更新根 README、硬件说明和最新验证记录；历史记录只在发现事实性错误时更正，并保留原始日期与版本边界。
