# 通信契约规则

修改前按需查看 [上位机协议 v1](https://github.com/zxu-source/Android_WIFI/blob/main/docs/protocol/wifi-control-v1.md)。它是双方契约；本文件只记录实施时必须维持的约束。

- API 基础路径 `/api/v1`：GET `device`、GET `state`、PUT `resources/{resourceId}/properties/{propertyId}`。响应为 UTF-8 JSON，`protocolVersion=1`，禁止缓存。
- `deviceId` 稳定，`bootId` 每次启动生成。`revision` 是本次启动内全局计数，仅在确认的状态变化时增加；同值写入不增加，不提供 toggle 或其他非幂等动作。
- 写请求要求 UUID `requestId`、匹配当前会话的 `expectedBootId` 和正确类型的 `value`。所有校验成功后才操作驱动；旧会话返回 409 DEVICE_RESTARTED。
- boolean 只接受 JSON boolean。不可将数字、字符串、null、数组或对象转换为 boolean。JSON 必须是单个完整对象；拒绝非标准语法、尾随垃圾和原始 NUL。未知可选字段允许存在。
- ID 只允许 ASCII 字母、数字、点、下划线和横线，长度 1–48；拒绝路径分隔符和编码绕过。路由不把资源 ID 当作 GPIO。
- 使用统一错误结构，无法解析或确认 requestId 时返回 null。错误 HTTP 状态和 code 遵循协议；405 带 Allow。资源/属性不存在和无效值必须明确报错，不能回退为默认输出。
- 在读 body 和解析 JSON 前执行长度校验；超限返回 413。固定缓冲区溢出不能截断成成功响应。
- 当前 HTTP 实现要求 Content-Length，不支持 chunked、Expect: 100-continue 或持久连接；拒绝这些形式，不等待无限长或不完整的请求。更改此限制时同步 README 与测试。
- 首版仅注册可读可写的 boolean 输出。后续只读值无有效采样时必须返回 null 与 unavailable，不能伪造 0、false 或 good。
- 不承诺持久 requestId 去重或 exactly-once。删除字段、改变类型/行为或引入非幂等动作前先处理协议版本和上位机兼容。
