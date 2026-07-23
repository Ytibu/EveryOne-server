# LogicSystem 接口文档

本文档基于 `EveryOne/logicsystem.cc` 的实际路由实现整理。当前实现中的接口分为 1 个 GET 和 2 个 POST 路由。

## 通用约定

- 所有 POST 接口当前使用 `Content-Type: text/json`
- 响应中的 JSON 错误码定义来自 `EveryOne/header.h`
- `error = 0` 表示成功
- 其余错误码含义如下：
  - `1001`：JSON 解析错误
  - `1002`：RPC 请求错误
  - `1003`：验证码过期
  - `1004`：验证码错误
  - `1005`：用户已存在
  - `1006`：密码不一致（两次密码输入不匹配）

## 1. GET /get_test

### 功能

用于联调和参数透传测试。接口会返回请求路径和所有 query 参数。

### 请求

- Method: `GET`
- Path: `/get_test`
- Query: 任意键值对

### 示例请求

```http
GET /get_test?a=1&b=hello HTTP/1.1
```

### 响应

- 成功时返回纯文本
- 响应体示例：

```text
receive get_test req
param 0: a=1
param 1: b=hello
```

### 说明

- 参数会按解析顺序输出
- 参数值会进行 URL 解码

## 2. POST /get_verify

### 功能

用于验证码查询测试。接口接收邮箱地址，调用 gRPC 验证码服务并返回结果。

### 请求

- Method: `POST`
- Path: `/get_verify`
- Body: JSON

### 请求体字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| email | string | 是 | 目标邮箱 |

### 示例请求

```json
{
  "email": "test@example.com"
}
```

### 成功响应

```json
{
  "error" : 0,
  "email" : "test@example.com"
}
```

### 错误响应

- JSON 解析失败：`error = 1001`，状态码 400，响应体含错误描述
- 缺少 `email` 字段：`error = 1001`，状态码 400，响应体含错误描述
- gRPC 调用失败时，`error` 字段映射为后端返回的错误码（通常为 `1002`）

### 说明

- 该接口把邮箱提交到 gRPC 验证码服务，并将 `error` 和 `email` 透传回前端
- proto 中 `GetVerifyRsp` 包含 `code` 字段，但当前实现未回传该字段
- 当前实现返回 `application/json`

## 3. POST /user_register

### 功能

用户注册接口。流程如下：

1. 解析请求 JSON
2. 校验 `password` 与 `confirm` 是否一致
3. 从 Redis 读取该邮箱对应的验证码
4. 比对验证码
5. 调用 MySQL 存储过程完成注册
6. 返回注册结果

### 请求

- Method: `POST`
- Path: `/user_register`
- Body: JSON

### 请求体字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| email | string | 是 | 注册邮箱 |
| user | string | 是 | 用户名 |
| password | string | 是 | 密码 |
| confirm | string | 是 | 确认密码 |
| verifycode | string | 是 | 验证码 |

### 示例请求

```json
{
  "email": "test@example.com",
  "user": "alice",
  "password": "P@ssw0rd",
  "confirm": "P@ssw0rd",
  "verifycode": "123456"
}
```

### 成功响应

```json
{
  "error" : 0,
  "uid" : 10001,
  "email" : "test@example.com",
  "user" : "alice",
  "password" : "P@ssw0rd",
  "confirm" : "P@ssw0rd",
  "verifycode" : "123456"
}
```

### 错误响应

| 场景 | error | 说明 |
| --- | --- | --- |
| JSON 解析失败 | 1001 | 请求体不是合法 JSON |
| `password != confirm` | 1006 | 两次密码输入不一致 |
| Redis 中验证码不存在 | 1003 | 验证码已过期或未写入 |
| 验证码不匹配 | 1004 | 请求验证码与 Redis 中存储的不一致 |
| MySQL 注册失败（用户/邮箱已存在等） | 1005 | 数据库存储过程返回失败 |

### 说明

- 请求必须包含 `email`、`user`、`password`、`confirm`、`verifycode` 五个字段，当前实现未做显式字段缺失检查（解析时缺失字段会得到空字符串，随后 `pwd != confirm` 可能误过或走到数据库层失败）
- 验证码 key 格式为 `code_{email}`，存储在 Redis 中
- 成功响应会回显 `password` 和 `confirm`，当前实现未做脱敏处理
- 当前实现返回 `application/json`

## 路由小结

| 路由 | 方法 | 用途 |
| --- | --- | --- |
| /get_test | GET | 参数测试 |
| /get_verify | POST | 获取验证码结果 |
| /user_register | POST | 用户注册 |
