# ESP32-S3-LCD Board (esp32s3-lcd-board) 适配说明

本分支针对 **ESP32_S3_LCD带屏幕** 开发板适配 xiaozhi-esp32（小智 AI 聊天机器人）。
板载：ST7735S 1.77 寸 LCD（128x160）、W5500 以太网、TF 卡、CH340K USB 串口（Type-C 供电）。

## 硬件信息（实测）

| 项目 | 值 |
|---|---|
| 芯片 | ESP32-S3 (rev v0.2)，双核 240MHz，Wi-Fi / BT 5 LE |
| Flash | Winbond W25Q128，16MB，Quad |
| PSRAM | ESP-PSRAM64H，8MB，Quad（运行时自动识别） |
| USB 串口 | CH340K，自动下载电路（DTR/RTS → EN/IO0） |
| LCD | ST7735S 1.77"，128x160，SPI，背光常亮（无 GPIO 控制） |
| 以太网 | W5500，SPI2（当前固件未启用） |
| TF 卡 | SDMMC 4-bit（GPIO33-38），已启用，挂载于 `/sdcard` |

## 引脚定义（main/boards/esp32s3-lcd-board/config.h）

| 功能 | GPIO |
|---|---|
| LCD CS / DC / RST / MOSI / SCK | 21 / 16 / 15 / 17 / 18 |
| BOOT 按键（配网/对话切换） | 0 |
| TF 卡 SDMMC：CMD / CLK / D0 / D1 / D2 / D3 | 35 / 36 / 37 / 38 / 33 / 34 |
| ES8311 I2C SDA / SCL（预留） | 1 / 2 |
| ES8311 I2S BCLK / WS / DIN / DOUT / LRCK（预留） | 3 / 4 / 6 / 7 / 9 |
| 板载 LED | 无（电源指示灯不受控） |

> 预留音频引脚为规划值，后期按实际音频模块接线调整即可。

## TF 卡（已支持）

TF 卡使用 SDMMC 外设 4-bit 模式，引脚 GPIO33-38（经 GPIO matrix 引出）。
**注意**：本板 PSRAM 为 Quad 模式（仅占用 GPIO26-32 与 Flash 共享总线），
因此 GPIO33-38 完全空闲；若把配置误改为 Octal PSRAM（`CONFIG_SPIRAM_MODE_OCT`），
该组引脚会被 PSRAM 占用，SD 卡将无法工作。

- 挂载点：`/sdcard`（FATFS，启动时自动挂载）
- 无卡时不阻塞启动，日志出现 `SD card mount failed (no TF card inserted)` 属正常
- 插入 TF 卡后重启，串口日志打印卡容量/速度等信息并显示 `SD card mounted at /sdcard`

## 环境要求

- Windows 10/11
- ESP-IDF **v6.1**（本项目 master 要求 >= 5.5.2）：
  - 源码：`C:\esp\v6.1\esp-idf`
  - 工具链与 Python 环境由 `install.bat` 安装至 `D:\Espressif`（IDF_TOOLS_PATH）
- Python 3.11（IDF 自带虚拟环境），辅助环境由 `uv` 管理

### 组件管理器补丁（必须）

ESP Component Registry 中 `cmake_utilities 0.5.0`、`esp_lcd_touch_ft5x06 1.0.6`、
`esp_lcd_touch_gt911 1.1.0` 的存储端元数据缺少 `checksums` 字段，`idf_component_tools 3.1.2`
读取时抛 `KeyError: 'checksums'`。已对本机装置打了兼容补丁：

- `idf_component_tools/registry/storage_client.py`：`checksums` 字段改为可选
- `idf_component_tools/sources/web_service.py`：`checksums_url` 为空时跳过校验文件下载

重新安装/升级该 Python 包后需重新打补丁：`python tools/patch_checksums.py`
（补丁文件已归档在 `tools/patch_checksums.py`）。

## 构建与烧录

本仓库提供了 Windows 下的构建包装脚本 `tools/run_idf.py`（解决 Git-Bash/MSYS 环境下
idf.py 拒绝运行的问题，自动导出 IDF 环境变量并走代理）。

```bash
# 首次构建（配置目标与板型，会自动下载 70+ 组件）
python tools/run_idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/esp32s3-lcd-board/sdkconfig.defaults" set-target esp32s3

# 编译
python tools/run_idf.py build

# 烧录（COM4）
python tools/run_idf.py -p COM4 flash

# 查看串口日志
python tools/run_idf.py -p COM4 monitor
```

板级配置作用：
- `CONFIG_BOARD_TYPE_ESP32S3_LCD_BOARD=y`：选择本板
- `CONFIG_LCD_ST7735_128X160=y`：128x160 屏幕
- `CONFIG_SPIRAM_MODE_QUAD=y`：本板 PSRAM 为 Quad（必须覆盖 IDF 默认的 OCT）
- 16MB Flash 分区表（`partitions/v2/16m.csv`）

产物：`build/xiaozhi.bin`、`build/bootloader/bootloader.bin`、`build/partition_table/partition-table.bin`、
`build/generated_assets.bin`（assets 分区分区）。

## 配网

首次开机进入配网模式：设备开启热点 `Xiaozhi-C54D`（MAC 尾 4 位），
手机连接后在浏览器访问 `http://192.168.4.1`，填写 WiFi 名称密码保存即可。
短按 BOOT 键可清空配置重新配网。

## 后期音频接入（ES8311 单麦 codec）

音频模块到位后按以下步骤启用：

1. 按实际接线修改 `config.h` 中 `AUDIO_CODEC_I2C_SDA/SCL_PIN`、`AUDIO_I2S_*` 引脚宏
2. 将 `esp32s3_lcd_board.cc` 中 `GetAudioCodec()` 的 `NoAudioCodecSimplex` 替换为
   `Es8311AudioCodec`（参考 `boards/nologo/xingzhi-abs-2.0/` 的实现，注意 I2C 总线初始化）
3. 重新编译烧录，验证唤醒词与语音通话

## 已知限制

- **W5500 以太网**：xiaozhi-esp32 master 仅支持 RMII 接口 PHY（如 IP101），
  W5500 为 SPI 接口网卡，需要自行移植（当前使用板载 Wi-Fi 联网）。
- **背光**：LEDA 直接接电源常亮，无亮度调节。