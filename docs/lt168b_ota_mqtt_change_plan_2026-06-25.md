# LT168B 屏幕联动 ESP8266 / OTA / MQTT 修改方案（修订版）

项目路径：`D:\Qian Start\Smart Watch\H743`

## 1. 当前目标

- LT168B 触摸屏点击版本更新键后，进入 OTA 查询流程。
- 屏幕任务先向 `CloudTask` 申请 OTA 独占，`CloudTask` 暂停 MQTT 并释放 ESP8266。
- 屏幕任务独占 ESP8266，通过 OneNET HTTP 接口查询 OTA 任务。
- 查询成功后进入版本更新页；查询失败则退出 OTA 演示态并恢复云端任务。
- 返回键退出 OTA 演示态，释放独占，`CloudTask` 恢复 MQTT。
- 当前工程的“开始更新”按钮走的是模拟升级流程，不是实际固件下载。

## 2. 关键文件

1. `App/lt_screen/lt_screen_task.c`
2. `App/lt_screen/lt_screen_keys.h`
3. `App/cloud/cloud_task.c`
4. `App/cloud/cloud_task.h`
5. `App/ota/ota_service.c`
6. `App/ota/ota_service.h`
7. `App/ota/ota_config.h`
8. `BSP/wifi/ESP8266.c`
9. `BSP/display/lt168b.c`
10. `BSP/display/lt168b.h`

## 3. 现有实现事实

### LT168B 键值

当前键值定义如下：

```c
#define LTSCREEN_KEY_VERSION_UPDATE     0x0003U
#define LTSCREEN_KEY_UPDATE_START       0x0021U
#define LTSCREEN_KEY_BACK               0x0022U
```

这几个值和当前代码一致，不需要再按旧记忆改成其他值。

### 屏幕事件判断

`lt_screen_is_key()` 不是只看 `address`，它同时判断：

```c
event->address
event->key_value
```

所以屏幕按键匹配必须同时兼容这两个字段。

### 页面与地址

当前版本页相关地址如下：

```c
#define LTSCREEN_CURRENT_VERSION_ADDR 0x0227U
#define LTSCREEN_LATEST_VERSION_ADDR  0x01C3U
#define LTSCREEN_UPDATE_PROGRESS_ADDR 0x02A0U
```

页面 ID 如下：

```c
#define LTSCREEN_VERSION_SAME_PAGE_ID   0x0005U
#define LTSCREEN_VERSION_UPDATE_PAGE_ID  0x0002U
```

`LT168B_GotoPage(page_id)` 本质上是向 `0x7000` 地址写页面号，高字节在前。

### OTA 独占机制

`CloudTask_RequestOtaExclusive(timeout_ms)` 的行为是：

- 置位 OTA 独占请求标志。
- 等待 `CloudTask` 停止 MQTT 并把 `s_cloud_ota_exclusive_ready` 置位。
- 超时则返回失败。

`CloudTask` 收到独占请求后，会执行：

- `ESP8266_CloseTcp()`
- `ESP8266_Clear()`
- `ESP8266_ClearTransportError()`
- `OneNet_ClearSessionError()`
- `OneNet_ResetSubscribeState()`

然后进入等待状态，直到屏幕侧调用 `CloudTask_ReleaseOtaExclusive()`。

### OTA 查询

`OTAService_QueryTask()` 当前逻辑是：

- 使用 `OTA_CURRENT_VERSION` 作为当前版本，当前值是 `V1.2`。
- 通过 `ESP8266_ConnectTcp(OTA_HTTP_HOST, OTA_HTTP_PORT)` 连接 OneNET HTTP。
- 发送 `/fuse-ota/<product>/<device>/check?type=<type>&version=<version>` 查询。
- 成功时返回 `OTA_SERVICE_UPDATED`，并把 `task.target` 写入最新版本。
- 没有任务时返回 `OTA_SERVICE_NO_UPDATE`。
- 出错时返回 `OTA_SERVICE_ERROR`。

当前配置里：

```c
#define OTA_HTTP_HOST    "iot-api.heclouds.com"
#define OTA_HTTP_PORT    80U
#define OTA_QUERY_TYPE   2U
```

`type=2` 是当前工程已经验证过的正确值，之前的 `12010 task type error` 属于 OneNET 参数问题，不是 ESP8266 TCP 问题。

### 更新按钮

`App/ota/ota_service.c` 里现在有：

```c
#define OTA_SIMULATE_UPGRADE_ONLY 1U
```

所以 `LTSCREEN_KEY_UPDATE_START` 对应的不是实际下载刷机，而是：

- 保存模拟升级信息。
- 触发一次重置。
- 下次按模拟升级路径处理。

也就是说，当前工程里的“开始更新”是演示流程，不是真正的固件写入流程。

## 4. 当前代码里最需要修正的点

1. 版本页文本现在写的是测试字符串，不是查询结果。
   目前代码在 `OTA_SERVICE_UPDATED` 分支里调用的是：

   ```c
   lt_screen_send_version_texts(LTSCREEN_CURRENT_VERSION_TEXT_TEST,
                                LTSCREEN_LATEST_VERSION_TEXT_TEST);
   ```

   这会让页面显示固定测试值，而不是 `current_version` / `latest_version`。

2. 版本页建议先跳页，再延时，再写文本。
   这样可以避免页面初始化把刚写入的文本覆盖掉。

3. `query failed` 分支现在会退出 OTA 演示态并释放独占，这一点是对的，应该保留。

4. `query no task` 分支现在只是跳到同页，没有主动释放独占。
   这意味着 MQTT 会继续暂停，直到用户按返回键退出演示态。
   如果你希望“没有任务就立刻恢复云端”，这里还要再改。

5. 返回键现在已经是 `0x0022U`，当前代码里不需要再改成旧值。

## 5. 推荐的执行顺序

```text
[版本更新键 0x0003]
  -> LTScreenTask 请求 OTA 独占
  -> CloudTask 停止 MQTT 并释放 ESP8266
  -> OTAService_QueryTask 通过 ESP8266 查询 OneNET
  -> 查询成功：进入版本更新页
  -> 查询无任务：停留在演示态或按需求直接退出
  -> 查询失败：释放独占并返回正常态

[返回键 0x0022]
  -> lt_screen_leave_ota_demo()
  -> CloudTask_ReleaseOtaExclusive()
  -> CloudTask 重建 MQTT
```

## 6. 当前代码里的注意点

- `lt_screen_leave_ota_demo()` 会清空 OTA 任务缓存并释放独占，返回键必须走这条路径。
- `LT168B_WriteText()` 直接按字符串长度发送，不会额外补 `\0`。
- `OTAService_QueryTask()` 不能按布尔值理解，只能按 `OTA_SERVICE_UPDATED` / `OTA_SERVICE_NO_UPDATE` / `OTA_SERVICE_ERROR` 三种状态处理。
- `OTA_SIMULATE_UPGRADE_ONLY = 1U` 时，更新流程只会走模拟升级，不会真正下载固件。

## 7. 建议的下一步

1. 把版本页写字内容改成查询返回值，而不是固定测试字符串。
2. 明确 `query no task` 是“保持 OTA 演示态”等待返回，还是“直接退出并恢复 MQTT”。
3. 如果后面要做真实 OTA，再单独把“模拟升级”和“真实下载”拆开。

