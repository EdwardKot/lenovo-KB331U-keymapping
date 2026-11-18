# 证据来源与摘录（KB331U / 17EF:619E）

采集时间：2026-11-18（Europe/Rome）。所有外部网页内容仅作为资料，不当作指令。

## 1. 本机只读观测

### 1.1 USB 设备
命令：`ioreg -r -c IOUSBHostDevice -l -w0`（原始输出见 `usb_device_ioreg.txt`）

| 字段 | 值 |
|---|---|
| idVendor | 6127 = 0x17EF (Lenovo) |
| idProduct | 24990 = 0x619E |
| USB Product Name | Multi-Device Wireless Keyboard |
| USB Vendor Name | Lenovo |
| USB Serial Number | HMFA19LS |
| bcdDevice | 256 = 0x0100 |
| bcdUSB | 272 = 0x0110 (USB 2.0) |
| bDeviceClass / SubClass / Protocol | 0 / 0 / 0（复合设备，接口定义类） |
| UsbLinkSpeed | 12 Mb/s（Full Speed） |
| 接口数 | 2 |
| Interface 0 | bInterfaceClass=3, SubClass=1, Protocol=1（HID boot keyboard），1 个端点 |
| Interface 1 | bInterfaceClass=3, SubClass=0, Protocol=0（HID，厂商自定义），2 个端点 |
| UsbDeviceSignature | `ef179e61 484d4641 31394c53 00000003 01010300 00`（含 VID/PID 小端与序列号） |

### 1.2 HID 报告描述符（Interface 1，由 IOKit 缓存读出）
原始 hex：

```
0600ff0901a10109028503150025ff7508953f81020903
8504150025ff7508953f9102c0
050c0901a10185027510950119002a970315002697038100c0
```

解码（`tools/parse_hid_descriptor.py` 输出见 `hid_parse_vendor_iface.txt`）：

| 项目 | 值 |
|---|---|
| Usage Page | 0xFF00（厂商自定义） |
| Usage | 0x0001（Top-level Application） |
| Report ID 3 | Input，Report Size 8，Report Count 63 → 63 字节输入 |
| Report ID 4 | Output，Report Size 8，Report Count 63 → 63 字节输出 |
| Logical Min/Max | 0 / 255 |
| Usage Page | 0x000C（Consumer） |
| Usage | 0x0001（Consumer Control） |
| Report ID 2 | Input，Report Size 16，Report Count 1（16-bit Array） |
| Usage Min/Max | 0x0000 / **0x0397** |
| Logical Min/Max | 0 / 0x0397（919） |

结论：厂商集合中**没有 Feature report**（无 0xB1 项），只有 Input(3)/Output(4)。Interface 0 另有一条标准键盘报告（Report ID 1：8 位修饰键 + 1 保留字节 + 5 位 LED + 6 键数组，Usage 0x00–0xA4）。

### 1.3 设备当前状态
- 通过 USB-C 有线连接（`locationID` 0x100000，USB Address 1），不是蓝牙连接。
- 系统中有 Chrome、ChatGPT 进程持有该 USB 设备的 user client（正常 HID 访问，非本次分析所为）。
- 本次分析**未向设备发送任何 HID 报告**，未执行 GET_REPORT/SET_REPORT，未进入 DFU。

## 2. HID Usage Tables 1.4（USB-IF）

- 文件：https://usb.org/sites/default/files/hut1_4.pdf
- Table 15.1（Consumer Page）末尾：
  - `2D3 Privacy Screen Level Minimum`
  - `2D4 Privacy Screen Level Maximum`
  - `2D5-4FF Reserved`
- 因此 **0x0397 落在 0x2D5–0x4FF 的 Reserved 区间，不是标准 Consumer Usage**。
- 描述符中的 0x0397 是 Usage Maximum / Logical Maximum，表示设备“可能上报 0–0x0397 的任意值”，并不等于某个具体按键。

## 3. FCC 认证资料（O57KB331U）

- 主页面：https://fccid.io/O57KB331U
- Grantee：Lenovo (Shanghai) Electronics Technology Co., Ltd
- 设备类别：Wireless Keyboard；频率 2402–2480 MHz；BT 5.3
- Test Report：https://fccid.io/O57KB331U/Test-Report/Test-Report-7353257.pdf
  - Hardware Version：**KA024_MB_V0.20**
  - Software Version：**KA024-A-0.09**
  - 供电：DC 3.8V 内置可充电电池
- Internal Photos：https://fccid.io/O57KB331U/Internal-Photos/Internal-Photo-7353247
  - 主板丝印 `KA024_MB V0.20`、`20231128`
  - 可见：40-pin FPC（键盘矩阵）、QFN 主控、若干未贴装测试点、电池引线
  - 分辨率 1008×673，芯片丝印不可辨（无法据此确认 MCU 型号）
- QSG：https://fccid.io/O57KB331U/User-Manual/QSG-7353240
  - 仅配对/充电/多设备切换说明，**无固件升级说明**
- User Manual（Safety & Warranty）：https://fccid.io/O57KB331U/User-Manual/user-manual-7353241
  - 仅安全、保修、法规信息

## 4. 官方固件升级渠道（关键发现）

### 4.1 ZUI 平板 OTA 明确包含“键盘固件升级”
- 联想小新 Pad 2024 的 ZUI 16.0.523 更新说明（2024-10-11）：
  “4. 支持对联想小新无线键盘 Air 进行固件升级”
  - https://m.douyin.com/share/note/7424309767801228584
  - 同一更新说明亦见：https://www.douyin.com/note/7424309767801228584
- ZUI 16.0.336（2024-08，Y700 2023）：“支持联想小新无线键盘 Air：支持快速配对键盘和多种快捷键”
  - https://www.ithome.com/0/787/733.htm
  - https://mefmobile.org/lenovo-legion-y700-2023-gets-zui-16-0-336-with-android-14-and-xiaoxin-keyboard-support/
- 联想官方兼容性列表（KB331U）：快速配对仅限 ZUI 15.1+ 的联想安卓平板
  - https://iknow.lenovo.com.cn/detail/420850

**推断**：KB331U 的官方固件升级由 ZUI 平板侧发起（键盘通常经蓝牙连接；是否也支持 USB-C 有线升级未验证）。没有公开的 PC 端独立升级器。

### 4.2 Lenovo Accessories and Display Manager（LADM）
- 联想外设/显示器管理平台，支持部分键鼠的固件更新。
- 2.0.1.1 更新说明列出的键鼠新品：Multi-Device Wireless Mouse 8000、Multi-Device Wireless Keyboard 8000、900 Multi-Device Wireless Mouse、900 Wireless Low-Profile Mechanical Keyboard。
- 来源：https://www.crxsoso.com/store/detail/xp8cjs4gf2r4qs
- **未发现 KB331U（17EF:619E）被 LADM 支持的证据**；这是有边界的负面结论，不是“绝对不支持”的证明。

### 4.3 fwupd 的 Lenovo Accessory 插件（对照，不是本设备）
- 文档：https://fwupd.github.io/libfwupdplugin/lenovo-accessory-README.html
- 源码：https://github.com/fwupd/fwupd/tree/main/plugins/lenovo-accessory
- 协议要点：
  - HID：**Get/Set Feature reports**，Report ID 0x00，64 字节，走 interface 3（`fu-lenovo-accessory-hid-common.h`）
  - 启动器 PID：`USB\VID_17EF&PID_6194`（common bootloader）
  - HID 为 **single-bank**：必须重启进 bootloader 才能写；写完再重启跳回应用
  - BLE 为 **dual-bank**：运行中写第二 bank，最后切换
  - 固件包：zip 内 `.bin` + 可选 `*.rsa-3072.sig`；impl 提供 `dfu_prepare / dfu_file / dfu_crc / dfu_signature_file / dfu_signature_verify / dfu_entry` 等命令
  - fwupd 2.1.8 发布说明：“Add RSA-3072 signature verification support for Lenovo accessories”
    https://github.com/fwupd/fwupd/releases/tag/2.1.8
- 与 KB331U 的差异：
  - KB331U 的厂商集合只有 Input(3)/Output(4)，**没有 Feature report**
  - 接口只有 2 个（0=键盘，1=厂商+Consumer），没有 interface 3
  - 619E 不在插件 quirk 列表（6194/6201/629D/62CC 等）
  - 因此 KB331U **不属于** fwupd 已实现的 Lenovo Accessory 协议族（至少当前版本不是）

### 4.4 LVFS
- 检索 `619e`：https://fwupd.org/lvfs/search?q=619e
- 未检索到对应固件/设备条目。
- LVFS API 匿名访问被拒绝（Permission denied），因此这是网页检索层面的负面结果。

## 5. 其他
- 17EF:619E 未收录于 usb.ids（rxtx.org 标注 “not in the usb.ids database”）。
  https://rxtx.org/device/17ef-619e
- 联想知识库仅有 KB331U 兼容性列表，未见固件/升级工具条目。
  https://iknow.lenovo.com.cn/detail/420850
- 联想历史案例（不同型号）表明其键盘固件工具可能是型号专用的 `KB_TP_Updater_V31.exe` 一类程序，并在 KB 文章中附带；KB331U 未找到对应文章。
  https://www.lenovo.com.cn/wiki/article-153880.html


## 6. v2 新增关键技术证据（2026-11-18）

### 6.1 主控芯片确认：Telink（泰凌微电子）QFN48 无线 SoC（`TLSR8278` / `TLSR8258` 系列）
1. **PCB 测试点丝印（`evidence/fcc/pcb_zoom.png`）**：
   - 主控芯片左侧 5 个测试点丝印：**`GND`、`TX`、`RX`、`3V`、`SWS`**；左上方 2 个测试点：**`DM`、`DP`**。
   - `SWS`（Single-Wire Slave / SWire）为泰凌微电子（Telink）专有单线烧录与调试接口。
2. **QFN48 物理引脚拓扑匹配（对照 Telink `TLSR8258F512ET48` / `TLSR8278ET48` Datasheet）**：
   - `PA<5>` (`DM`)、`PA<6>` (`DP`)、`PA<7>` (`SWS`, Pin 9) 位于芯片左侧边；
   - `XC1` (Pin 34)、`XC2` (Pin 33) 位于芯片下侧边，外接 24 MHz 四脚贴片晶振；
   - `ANT` (Pin 40) 位于芯片右侧边，经 $\pi$ 型匹配网络接射频测试座及 `KA024-BT-BRD-V0.3-2402` 天线；
   - 符合 FCC 报告中 Bluetooth 5.3（2402–2480 MHz）规格。
3. **USB 控制器硬件指纹（`evidence/usb_device_ioreg.txt`）**：
   - `UsbLinkSpeed = 12000000`（Full-Speed 12 Mbps），但 `bcdUSB = 0x0110` 且 **`bMaxPacketSize0 = 8`**（Telink 硬件 USB 控制器 Endpoint 0 FIFO 固定为 8 字节）。
   - Interface 0 键盘报告描述符（含 `81 02 81 03` 及 9-bit LED + 7-bit padding、`26 a4 00`）与 Telink BLE SDK `application/usbstd/usbdesc.c` 标准实现一致。

### 6.2 Android 标准最近任务（`keyevent 187` / `KEYCODE_APP_SWITCH`）的 HID Consumer 映射链路
1. **Linux 内核 `drivers/hid/hid-input.c`（Consumer Page `0x0C`）**：
   - `case 0x1a2: map_key_clear(KEY_APPSELECT); break;`（Consumer Usage `0x01A2` `AL Select Task/Application` $\rightarrow$ Linux keycode `580` `KEY_APPSELECT`）
   - `case 0x29f: map_key_clear(KEY_SCALE); break;`（Consumer Usage `0x029F` `AC Desktop Show All Windows` $\rightarrow$ Linux keycode `120` `KEY_SCALE`）
2. **Android AOSP `/system/usr/keylayout/Generic.kl`**：
   - `key 580 APP_SWITCH`（对应 `KeyEvent.KEYCODE_APP_SWITCH` = `187`）
   - `key 120 APP_SWITCH`（对应 `KeyEvent.KEYCODE_APP_SWITCH` = `187`）
3. **固件二进制改键最小化改动结论**：
   - 互联键原厂发送 Consumer Usage `0x0397`（小端 `97 03`，Report ID 2）。
   - 描述符声明 `Usage Minimum = 0x0000, Usage Maximum = 0x0397, Logical Maximum = 0x0397`。
   - 将固件 Consumer 键表中的 `97 03`（`0x0397`）替换为 **`a2 01`（`0x01A2`）** 或 **`9f 02`（`0x029F`）**，无需改动 HID 描述符即可在 Android（含 HONOR MagicPad 4）有线与蓝牙模式下原生触发 `keyevent 187`。

### 6.3 联想 ZUI 侧键盘升级与管理组件
- **`com.zui.udevice`**（Smart Accessories / 智能外设服务，`ZuiUDevice.apk`）
- **`com.zui.keyboardupdate`** / **`com.zui.keyboardupdate.olympia`**（Keyboard Firmware Update）
- 搭载于小新 Pad 2024（`TB331FC`，`ZUI 16.0.523+`）与拯救者 Y700 2023（`TB320FC`，`ZUI 16.0.336+`）固件中（公开镜像源：`mirrors.lolinet.com/firmware/lenowow/`）。

---

## 7. v3 突破性证据：官方 APK 逆向、原厂固件拉取与键表偏移定位（2026-11-18）

### 7.1 `com.zui.keyboardupdate.olympia` 反编译核心结论
样本：`evidence/com.zui.keyboardupdate.olympia_V...250616-2_minAPI31(arm64-v8a,armeabi-v7a)(nodpi)_apkmirror.com.apk`

1. **目标设备 100% 匹配（`DeviceEntryMainActivity.java` 第 728–737 行）**：
   - `VID = 6127` (`0x17EF`), `PID = 24990` (`0x619E`)
   - `DEFAULT_BIN = "KA024_A_0.0.9.bin"`
   - `OTA_SERVER_MODEL_SUFFIX = "OLYMPIA"`
2. **USB HID 升级协议（`a/e.java` 与 `a/a.java`）**：
   - 使用 USB **Interface 1** 的 Interrupt/Bulk OUT 端点（Report ID `0x04`）和 IN 端点（Report ID `0x03`），64 字节缓冲区。
   - **两种 CRC-16 算法（`a/a.java`）**：
     - `a.a(byte[])`（外层帧 CRC-16，大端存储于帧尾 2 字节，计算范围 `frame[1 .. len-3]`）：`init = 0x2A6E` (`10862`), `poly = 0x8005` (`32773`), MSB-first。
     - `a.b(byte[])`（内层 Telink OTA CRC-16/MODBUS，小端存储）：`init = 0xFFFF`, `poly = 0xA001`, LSB-first。
   - **指令集**：
     1. **只读查询固件版本**：发送 7 字节 `04 03 04 01 01 35 02`（补零至 64 字节），设备通过 Report ID `0x03` 返回 `03 04 04 <len> <ASCII 版本号>`。
     2. **启动 OTA（`CMD_OTA_START = 0xFF01`）**：发送 8 字节 `04 03 01 02 01 FF 44 80`，设备返回 `03 04 01 ... 01`。
     3. **写入 16 字节固件分块（共 `4033` 块）**：发送 26 字节 `04 03 02 14 <idx_lo> <idx_hi> <16B data> <inner_crc16_le> <outer_crc16_be>`，每包间隔 5 ms 并读取 `03 04 02 ...` 确认（`bArr8[4] == 1 || bArr8[4] == 10`）。
     4. **结束 OTA（`CMD_OTA_END = 0xFF02`）**：发送 14 字节 `04 03 03 08 02 FF <idx_lo> <idx_hi> <~idx_lo> <~idx_hi> <inner_crc16_le> <outer_crc16_be>`。
3. **BLE 蓝牙 OTA 升级协议（`BtMainModule.java` 与 `com.telink.ota.ble.OtaController`）**：
   - 标准 Telink BLE OTA，无 RSA/加密签名（`OtaSetting.secureBootEnable = false`）。
   - `OtaController.checkCRC()`（第 339–355 行）使用 `com.telink.ota.util.Crc.calCrc32()` 校验固件前 `len - 4` 字节的 CRC-32（`~zlib.crc32(fw[:-4]) & 0xFFFFFFFF`）是否与末尾 4 字节小端值一致。

### 7.2 联想官方 OTA 服务器返回原厂固件 `KA024_A_1.0.8`
通过 `MainActivity.java` 中的查询接口请求 `https://ota.lenovo.com/engine/upgrade?devicemodel=TB373FU_OLYMPIA_PRC&deviceid=HMFA19LS&curfirmwarever=TB373FU_OLYMPIA_S000002_PRC&action=querynewfirmware&locale=en`（脚本：`tools/query_lenovo_ota.py`，响应存档：`evidence/ota_response.xml`）：

- **官方 CDN 下载地址**：`https://ota-cdn.lenovo.com/firmware/20241018173629694-4240.bin`
- **本地存档**：`evidence/KA024_official_firmware.bin`
- **文件大小**：`64,516` 字节（`0xFC04` 字节）
- **MD5**：`907767c2ecbb6381014404c39e0b95e5`
- **头部特征**：`0x08..0x0B` = `4B 4E 4C 54` (`"KNLT"`, Telink 启动魔数)；`0x18..0x1B` = `04 FC 00 00` (`0x0000FC04` 字节)。
- **尾部 28 字节（`0xFBE8..0xFC03`）**：
  - `0xFBE8..0xFBFF`：`"KA024_A_1.0.8"` + 11 字节 `0x00` 填充
  - `0xFC00..0xFC03`：`8d 8a a3 87`（小端 `0x87A38A8D`，与 `~zlib.crc32(fw[:-4]) & 0xFFFFFFFF` 计算结果 `0x87A38A8D` **100% 吻合**）

### 7.3 原厂固件 `KA024_official_firmware.bin` 中的按键映射表与发送函数定位
1. **顶部多媒体/快捷功能键 Consumer Usage 映射表（偏移 `0xEE44..0xEE6D`，7 项 × 6 字节，由函数 `0x4CCC` 在 `0x4DFC` 处引用）**：
   | 文件偏移 | 原始字节 (6B) | 内部键码 (`key_code`) | Consumer Usage (`uint16_t` LE) | 标志 (`flag`) | 对应物理按键 |
   |---|---|---|---|---|---|
   | `0xEE44` | `eb 00 e2 00 01 00` | `0x00EB` | `0x00E2` (Mute) | `0x0001` | 静音键 |
   | `0xEE4A` | `ec 00 ea 00 01 00` | `0x00EC` | `0x00EA` (Volume Decrement) | `0x0001` | 音量减 |
   | `0xEE50` | `ed 00 e9 00 01 00` | `0x00ED` | `0x00E9` (Volume Increment) | `0x0001` | 音量加 |
   | `0xEE56` | `ee 00 70 00 01 00` | `0x00EE` | `0x0070` (Brightness Decrement) | `0x0001` | 亮度减 |
   | `0xEE5C` | `ef 00 6f 00 01 00` | `0x00EF` | `0x006F` (Brightness Increment) | `0x0001` | 亮度加 |
   | `0xEE62` | `f1 00 8e 03 01 00` | `0x00F1` | `0x038E` (截图/Snipping，在 `0x4CFE` 特殊判断：Mac/Win 下转标准键盘截图快捷键) | `0x0001` | 截图键 |
   | **`0xEE68`** | **`f2 00 97 03 01 00`** | **`0x00F2`** | **`0x0397`（偏移 `0xEE6A..0xEE6B` = `97 03`）** | `0x0001` | **右上角设备互联键（Device Interconnection）** |

2. **USB 与 BLE 双模共用同一张表（函数 `0x4CCC..0x4D36`）**：
   - 当未按住 `Fn` 键时，函数 `0x4CCC` 遍历 `0xEE44` 处的 7 项表，查出 `0x00F2` 对应的 16-bit Consumer Usage（偏移 **`0xEE6A`**）。
   - 在有线 USB 模式下调用 `0x6220`（`0x4D08`）通过 Report ID `0x02` 发送该 16-bit 值；在蓝牙 BLE 模式下调用 `0x626C`（`0x4D32`）通过 BLE HOGP Report ID `0x02` 发送同一个 16-bit 值。
   - USB Consumer 描述符（`0xEB6C`，范围 `0x0000..0x0397`）与 BLE Consumer 描述符（`0xF0FD`，范围 `0x0001..0x0398`）均原生支持 `0x01A2`（`a2 01`）与 `0x029F`（`9f 02`）。
   - 当按住 `Fn` 键时，函数 `0x4CCC` 改查 `0xEF34` 处的 `Fn` 层映射表（`0xEF6A`: `f2 00 71 00 00 00`），因此修改 `0xEE6A` 处的 2 字节不会影响 `Fn` 组合键及任何其他按键。
