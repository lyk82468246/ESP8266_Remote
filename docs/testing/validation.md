# 验证记录

日期：2026-10-09。开发基线：NodeMCU/ESP-12E，GPIO2 板载 LED，低电平有效；用户同意先采用该配置，未核验实物板。

| 检查 | 结果 | 证据/范围 |
| --- | --- | --- |
| ESP8266 AP 固件编译 | 通过 | PlatformIO Core 6.1.18；espressif8266 4.2.1；Arduino Core 3.1.2；ArduinoJson 6.21.5 |
| ESP8266 STA 固件编译 | 通过 | `tools/pio.ps1 run -e nodemcuv2_sta`；RAM 44500 / 81920；Flash 293715 / 1044464；网络连接未实测 |
| 构建静态容量 | 通过 | RAM 44468 / 81920（54.3%）；Flash 294147 / 1044464（28.2%）。运行时堆与无线负载待实测 |
| C++ 契约测试 | 通过 | `tools/test_native.ps1`；MinGW 5.1.0；179 项 API/HTTP/驱动替身断言 |
| 联调 Python 脚本 | 通过语法与入口检查 | `py_compile` 及 `--help`；未连接真实服务器运行 |
| 烧录、串口热点及真实 LED | 未执行 | 未使用真实板，不以本机 GPIO 替身测试替代实物证据 |
| Android 真机联调 | 未执行 | 上位机仓库目前提供待实现协议/设计，本次没有修改其源码 |

契约断言包括：启动关闭、能力来自注册表、状态/quality、明确目标写入、同值写 revision 不变、低电平映射、错误请求无 GPIO 写入、旧 bootId、关联 requestId、类型拒绝、媒体类型、非法/未知 ID、405 Allow、字段缺失、严格 JSON、尾随垃圾及嵌入 NUL、1024/1025 字节边界、重复长度/非法头/chunked/Expect、不完整帧和驱动错误还原。

固件构建工具的 `elf2bin.py` 在 Python 3.12 下产生两条第三方正则转义 SyntaxWarning，构建成功；项目 C++ 编译未报告警告。没有执行烧录，也没有联网控制真实输出。

构建产物：`.pio/build/nodemcuv2/firmware.bin`（忽略目录）。真实板联调命令见 README；需人工观察 LED 并记录串口地址、手机型号与网络条件。
