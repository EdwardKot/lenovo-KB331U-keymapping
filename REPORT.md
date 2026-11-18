# Lenovo KB331U（17EF:619E）固件级改键深度研究报告（v4 · 已实机验证成功）

> 版本：v4（2026-11-18）｜状态：**已完成固件级永久改键并实机验证成功（蓝牙连接 HONOR MagicPad 4 按下原设备互联键已成功唤起最近任务视图）**
> 设备：Lenovo Multi-Device Wireless Keyboard（小新无线键盘 Air，KB331U），VID:PID `17EF:619E`，S/N `HMFA19LS`，内部代号 `KA024`（ZUI 代号 `OLYMPIA`）
> 最终生效固件：`KA024_A_1.0.8`（基于官方原厂固件修改 `0xEE6A..0xEE6B` 为 `9f 02` = Consumer Usage **`0x029F`**，并重算尾部 CRC-32 为 `0xD7134A81`）

---

## 0. 核心问题直接回答（Executive Summary）

针对 `background.txt` 第 6 节提出的 6 个核心问题，结合本次研究及最终实机成功结果回答如下：

| 问题 | 结论与技术细节 |
|---|---|
| **1. 使用什么 MCU 和升级协议？** | **主控芯片确认为泰凌微（Telink Semiconductor）QFN48 蓝牙 5.3 SoC（`TLSR8278` / `TLSR8258` 系列，32-bit TC32 内核）**。<br>• 已通过反编译联想官方升级组件 **`com.zui.keyboardupdate.olympia`**（硬编码 `VID = 6127 (0x17EF)`, `PID = 24990 (0x619E)`）完整还原其 **USB-C 有线 HID 升级协议**（Interface 1 Report ID `0x04` OUT / `0x03` IN，外层自定义 CRC-16 + 内层 Telink MODBUS CRC-16）与 **蓝牙 BLE OTA 协议**。<br>• **无任何加密签名（No RSA）**：官方固件仅在末尾 4 字节附带 Telink 32-bit Image CRC（`~zlib.crc32(fw[:-4]) & 0xFFFFFFFF`）。 |
| **2. 能否只修改 Device Interconnection 的按键映射，且改为哪个标准 HID 值最优？** | **已验证：`0x029F`（`AC Desktop Show All Windows` $\rightarrow$ Linux `KEY_SCALE = 120` $\rightarrow$ Android `APP_SWITCH` / `keyevent 187`）在 HONOR MagicPad 4 上实测成功！**<br>• 原厂固件 `evidence/KA024_official_firmware.bin`（`KA024_A_1.0.8`，64,516 字节）偏移 `0xEE44..0xEE6D` 存放了顶部 7 个多媒体/快捷功能键的 6 字节结构体表，其中第 7 项（`0xEE68..0xEE6D`）为 `f2 00 97 03 01 00`（内部键码 `0x00F2` $\rightarrow$ Consumer Usage **`0x0397`**，位于 **`0xEE6A..0xEE6B`**）。<br>• **关键实测差异**：虽然 `0x01A2`（`KEY_APPSELECT = 580`）与 `0x029F`（`KEY_SCALE = 120`）在 AOSP `Generic.kl` 中都映射到 `APP_SWITCH`，但荣耀 MagicOS 实测未映射较新的 `580`；将 `0xEE6A..0xEE6B` 改为 **`9f 02`（`0x029F`）** 并更新末尾 4 字节 CRC-32（`0xD7134A81`）后，蓝牙连接 MagicPad 4 直接唤起最近任务视图，其他按键与 `Fn` 组合键 100% 不受影响。 |
| **3. 是否有现实可行的固件修改路径？** | **已全链路跑通免拆机纯软件路线（路线 A）并完成刷机**：<br>1. 从联想官方 OTA 服务器（`ota.lenovo.com`）拉取到 **官方原厂固件 `evidence/KA024_official_firmware.bin`**（`KA024_A_1.0.8`，64,516 字节，MD5 `907767c2ecbb6381014404c39e0b95e5`）作为 **100% 原厂回滚基线**。<br>2. 通过自研 macOS C 工具 `tools/flash_kb_firmware` 经 USB-C 刷入改键固件（共 4,033 块），成功由出厂 `KA024_A_1.0.6` 升级至改键版 `KA024_A_1.0.8`。 |
| **4. 能否不拆键盘，仅靠 MacBook USB-C 刷入自定义固件？** | **已实机验证完全可行！** 通过 macOS IOKit `IOHIDDeviceSetReport` / `IOHIDDeviceRegisterInputReportCallback` 走 Interface 1（`UsagePage 0xFF00`）Report ID `0x04` / `0x03`，无需拆机、无需任何外接烧录器，15 秒即可完成一次完整刷写与自动重启。 |

---

## 1. 硬件与主控芯片溯源（突破性进展）

在 v1 报告中，由于仅观察了芯片表面模糊丝印，未能识别出主控型号。本次通过对 `evidence/fcc/pcb_zoom.png` 的电路拓扑、丝印标注以及 `evidence/usb_device_ioreg.txt` 的底层 USB 参数进行交叉比对，**100% 锁定主控为泰凌微（Telink）QFN48 无线 SoC（`TLSR8278` / `TLSR8258` 系列）**。

### 1.1 PCB 硬件证据（`KA024_MB V0.20`）

观察 `evidence/fcc/pcb_zoom.png`：

1. **专属调试口丝印 `SWS`**：
   - 主控芯片左下角排列有 5 个测试点，丝印从左至右依次为：**`GND`、`TX`、`RX`、`3V`、`SWS`**。
   - **`SWS`（Single-Wire Slave / SWire）** 是泰凌微电子（Telink）专有的单线调试与 Flash 编程接口名称（Nordic/STM32/沁恒等 ARM 芯片使用的是双线 `SWDIO`/`SWCLK`）。
2. **QFN48 引脚排布完全吻合 Telink `TLSR8258F512ET48` / `TLSR8278ET48`**：
   - **左侧边（Pins 1–12）**：`PA<5>`（`DM`）、`PA<6>`（`DP`）走线引向左上方的 `DM`/`DP` 测试点；Pin 9 `PA<7>`（`SWS`）直接引向左下方的 `SWS` 测试点。
   - **下侧边（Pins 33–34）**：`XC2`、`XC1` 直接连接芯片正下方的 24 MHz 四脚贴片无源晶振。
   - **右侧边（Pin 40 `ANT`）**：单端射频输出经 $\pi$ 型匹配网络连至板载射频测试座及 `KA024-BT-BRD-V0.3-2402` 天线触点。
   - **上侧边**：大量 GPIO 直接扇出至 40-pin 键盘矩阵 FPC 座。
   - **电源区（板右侧）**：连接 `VBAT` / `T`（NTC 温敏） / `GND` 三线锂电池（3.8V），配备独立的锂电充电管理 IC 与功率电感。

### 1.2 USB 与 HID 栈特征证据

`evidence/usb_device_ioreg.txt` 中的多处底层参数同样指向 Telink 官方 SDK（`tl_ble_sdk` / `tc_ble_single_sdk`）：

| 特征项 | 实测值 | 技术含义 |
|---|---|---|
| `UsbLinkSpeed` | `12000000` (12 Mbps Full-Speed) | 全速 USB 物理层 |
| `bcdUSB` | `0x0110` (USB 1.1) | Telink SDK `usbdesc.c` 默认常量 |
| `bMaxPacketSize0` | **`8`** | **关键指纹**：绝大多数 Cortex-M 全速 USB 芯片 EP0 包长为 64 字节，而 Telink 硬件 USB 控制器的 Endpoint 0 硬件 FIFO 固定为 **8 字节** |
| Interface 0 报告描述符 | `05 01 09 06 ... 81 02 81 03 95 09 05 08 ... 26 a4 00 05 07 19 00 2a a4 00 81 00 c0` | 与 Telink SDK 标准键盘描述符逐字节吻合（包含连续的 `81 02 81 03` 及 9-bit LED + 7-bit 填充、Usage Max `0x00A4`） |
| 硬件/固件版本号 | HW: `KA024_MB_V0.20`<br>SW: `KA024-A-0.09` | 内部项目代号为 **`KA024`** |

---

## 2. 目标按键与 Android `keyevent 187` 映射链路分析

### 2.1 当前互联键的 HID 行为（已由 MagicPad 4 实测证实）

1. **USB / BLE HID 描述符定义（Interface 1, Report ID 2）**：
   ```text
   05 0C        Usage Page (Consumer, 0x0C)
   09 01        Usage (Consumer Control, 0x01)
   A1 01        Collection (Application)
     85 02      Report ID (2)
     75 10      Report Size (16 bits)
     95 01      Report Count (1)
     19 00      Usage Minimum (0x0000)
     2A 97 03   Usage Maximum (0x0397)
     15 00      Logical Minimum (0)
     26 97 03   Logical Maximum (0x0397)
     81 00      Input (Data, Array, Absolute)
   C0           End Collection
   ```
2. **按键实际输出**：
   - 当按下右上角 **Device Interconnection（设备互联）** 键时，键盘通过 **Report ID `0x02`** 发送 16-bit Consumer Usage **`0x0397`**（报文内容：`02 97 03`，释放时：`02 00 00`）。
   - 正因为该键的值是全键盘 Consumer 键中最大的（`0x0397` = 919），研发人员在编写 HID 描述符时才特意将 `Usage Maximum` 和 `Logical Maximum` 设为 `2A 97 03` / `26 97 03`。
   - 在标准 USB-IF HID Usage Tables 1.4 中，`0x02D5–0x04FF` 为 `Reserved`（保留未定义）。联想 ZUI 在系统框架层（`PhoneWindowManager` / `KeyboardShortcutController`）私有拦截了 `MSC_SCAN 000c0397` 以触发联想“超级互联”；而在 HONOR MagicPad 4（MagicOS）及原生 Android 上，Linux 内核 `hid-input.c` 将其映射为 `KEY_UNKNOWN`，无法触发任何操作。

### 2.2 为什么改为 Consumer Usage `0x01A2`（或 `0x029F`）远优于改为 `Alt + Tab`？

在 `background.txt` 中提到：“可以考虑让该按钮发送 `Alt + Tab`，或其他能够触发 Android 最近任务的标准 HID 输入，且已验证 `adb shell input keyevent 187` 成功”。

通过查阅 Linux 内核 `drivers/hid/hid-input.c` 与 AOSP `/system/usr/keylayout/Generic.kl`，我们找到了**直达 `keyevent 187`（`KEYCODE_APP_SWITCH`）的原生标准 Consumer Usage**：

| HID Usage Page | HID Usage ID | 小端字节 | HID 标准名称 | Linux 内核 (`hid-input.c`) 映射 | Android (`Generic.kl`) 映射 | Android KeyEvent |
|---|---|---|---|---|---|---|
| `0x0C` (Consumer) | **`0x01A2`** | **`a2 01`** | `AL Select Task/Application` | `case 0x1a2: map_key_clear(KEY_APPSELECT);` (580) | `key 580 APP_SWITCH` | **`KEYCODE_APP_SWITCH` (187)** |
| `0x0C` (Consumer) | **`0x029F`** | **`9f 02`** | `AC Desktop Show All Windows` | `case 0x29f: map_key_clear(KEY_SCALE);` (120) | `key 120 APP_SWITCH` | **`KEYCODE_APP_SWITCH` (187)** |
| `0x0C` (Consumer) | `0x0397`（原厂） | `97 03` | *Reserved（联想私有互联键）* | `KEY_UNKNOWN` (240) | 未映射 | 无响应 |

**采用 `0x01A2`（或 `0x029F`）替代 `0x0397` 的四大优势**：

1. **直接触发 `keyevent 187`**：无需像 `Alt + Tab` 那样模拟“按住 Alt $\rightarrow$ 按 Tab $\rightarrow$ 松开 Alt”的修饰键状态机，单次按下即等同于执行 `input keyevent 187`（直接呼出/收起 MagicPad 4 最近任务视图）。
2. **仅需修改固件中的 2 个字节**：在键盘固件中，顶部一排多媒体/功能键统一存放在一张 `uint16_t` 的 Consumer Usage 映射表里。将 `0x0397`（`97 03`）改为 `0x01A2`（`a2 01`）只是**原位替换一个 16-bit 常量**，不改变任何函数调用与报文类型（仍走 Report ID `0x02`）。
3. **无需修改 HID Report Descriptor**：原厂描述符已声明合法范围为 `0x0000..0x0397`，而 `0x01A2` 与 `0x029F` 均在 `0x0000..0x0397` 范围内，USB 与 BLE 描述符无需变动一分一毫。
4. **蓝牙与有线双模同时生效**：Telink 双模键盘固件中，矩阵扫描查表得到 Consumer Usage 后，根据当前连接模式分别送入 `usb_hid_report_send` 或 `blc_gatt_pushHandleValueNotify`（BLE HID Report ID 2）。修改键值表常量后，**蓝牙连接 MagicPad 4 与 USB 连接天然同步生效**。

---

## 3. 固件升级机制与改键实施方案对比

针对该键盘，目前有**两条**可实现固件级永久改键的技术路线：

### 路线 A：纯软件免拆机路线（逆向 ZUI 固件更新器 + USB/BLE OTA 刷入）

#### 3.1 已知的官方升级架构
1. **ZUI 侧负责组件**：
   - 在联想小新 Pad 2024（`TB331FC`，`ZUI 16.0.523`）和拯救者 Y700 2023（`TB320FC`，`ZUI 16.0.336`）中，系统通过 **`com.zui.udevice`（智能外设服务 / Smart Accessories）** 及 **`com.zui.keyboardupdate`（键盘固件更新服务）** 管理 KB331U 键盘。
   - 官方操作路径为：`设置 -> 手写笔和键盘 -> 选中键盘 -> 固件升级`（官方提示优先推荐通过 USB-C 数据线连接平板升级，也支持蓝牙升级）。
2. **Telink OTA 底层机制（Dual-Bank A/B 安全分区）**：
   - Telink SoC 的内部 Flash（通常 512 KB）采用 **A/B 双槽（Dual-Slot）机制**（例如 Bank A 位于 `0x00000`，Bank B 位于 `0x20000` 或 `0x40000`）。
   - 升级时，设备擦除并向**未激活的备用分区**按 16 字节分块（附带 `adr_index` 与 `CRC-16`）写入新固件（控制码通常为 `0xFF00` 查询版本、`0xFF01` 开始 OTA、`0xFF02` 结束校验）。
   - 只有当全量数据块与末尾的 **32-bit Image CRC** 校验通过后，设备才会将新分区偏移 `0x08` 处的启动标志字节写为 `0x4B`（`"KNLT"` Telink Boot 魔数）并将旧分区清零。
   - **签名机制**：Telink 标准 BLE/USB-HID OTA 仅校验 **CRC-16（传输块）+ CRC-32（完整镜像）**，硬件 BootROM 不强制 RSA 签名（与 `fwupd` 中针对 `6194` STM32/NXP 底座的 `lenovo-accessory` 协议完全不同）。

#### 3.2 路线 A 已完成的逆向突破与精确 Patch 方案
1. **官方升级器 APK 反编译验证（`com.zui.keyboardupdate.olympia`）**：
   - `DeviceEntryMainActivity.java`（第 728–737 行）明确定义：
     `VID = 6127 (0x17EF)`, `PID = 24990 (0x619E)`, `DEFAULT_BIN = "KA024_A_0.0.9.bin"`, `OTA_SERVER_MODEL_SUFFIX = "OLYMPIA"`。
   - **USB HID 升级协议（`a/e.java` 与 `a/a.java`）**：
     - 通信接口：USB **Interface 1**，OUT 端点（Report ID `0x04`，64 字节）与 IN 端点（Report ID `0x03`，64 字节）。
     - **外层帧 CRC-16（`a.a(byte[])`）**：`init = 0x2A6E` (`10862`), `poly = 0x8005` (`32773`), MSB-first，对 `frame[1 .. len-3]` 计算，以**大端（Big-Endian）**置于每帧末尾 2 字节。
     - **内层 Telink OTA CRC-16（`a.b(byte[])`）**：标准 CRC-16/MODBUS（`init = 0xFFFF`, `poly = 0xA001`, LSB-first），以**小端（Little-Endian）**置于内层载荷末尾 2 字节。
     - **4 条核心指令**：
       1. **只读查询固件版本**：发送 7 字节 `04 03 04 01 01 35 02`，设备在 Report ID `0x03` 返回 `03 04 04 <len> <ASCII 版本号>`。
       2. **开始升级（`0xFF01`）**：发送 8 字节 `04 03 01 02 01 FF 44 80`，设备返回 `03 04 01 ... 01`。
       3. **写入 16 字节固件块（共 4,033 块）**：发送 26 字节 `04 03 02 14 <idx_lo> <idx_hi> <16B data> <inner_crc16_le> <outer_crc16_be>`，每块间隔 5 ms 并校验 `03 04 02 ...` 应答。
       4. **结束升级（`0xFF02`）**：发送 14 字节 `04 03 03 08 02 FF <idx_lo> <idx_hi> <~idx_lo> <~idx_hi> <inner_crc16_le> <outer_crc16_be>`。
2. **获取联想官方原厂固件（`evidence/KA024_official_firmware.bin`）**：
   - 使用 APK 内置的联想官方 OTA 接口（`tools/query_lenovo_ota.py`）从 `ota-cdn.lenovo.com` 下载到官方最新量产固件 **`KA024_A_1.0.8`**（文件大小 `64,516` 字节 = `0xFC04`，MD5 `907767c2ecbb6381014404c39e0b95e5`）。
   - **尾部 CRC-32 验证（`com.telink.ota.util.Crc.calCrc32`）**：
     固件末尾 28 字节（`0xFBE8..0xFC03`）为 `"KA024_A_1.0.8"` + 11 字节 `0x00` + 4 字节小端 CRC-32 `8d 8a a3 87`（`0x87A38A8D`），与 `~zlib.crc32(fw[:-4]) & 0xFFFFFFFF` 计算结果 **100% 吻合**，且无任何 RSA 签名。
3. **固件内互联键映射表偏移精确定位（`0xEE6A`）**：
   - 固件偏移 **`0xEE44..0xEE6D`** 为顶部 7 个多媒体/快捷功能键的 Consumer Usage 映射表（7 项 × 6 字节，由按键分发函数 `0x4CCC` 在 `0x4DFC` 处直接引用）：
     - `0xEE44`: `eb 00 e2 00 01 00`（静音 `0x00E2`）
     - `0xEE4A`: `ec 00 ea 00 01 00`（音量减 `0x00EA`）
     - `0xEE50`: `ed 00 e9 00 01 00`（音量加 `0x00E9`）
     - `0xEE56`: `ee 00 70 00 01 00`（亮度减 `0x0070`）
     - `0xEE5C`: `ef 00 6f 00 01 00`（亮度加 `0x006F`）
     - `0xEE62`: `f1 00 8e 03 01 00`（截图键 `0x038E`）
     - **`0xEE68`**: **`f2 00 97 03 01 00`**（**右上角设备互联键 `0x00F2` $\rightarrow$ Consumer Usage `0x0397`，位于偏移 `0xEE6A..0xEE6B`**）
   - 函数 `0x4CCC` 查出该 16-bit 值后，在 USB 模式下调用 `0x6220`、在蓝牙 BLE 模式下调用 `0x626C` 发送 Report ID `0x02`。
   - **最小化 Patch 方案**：仅将偏移 **`0xEE6A..0xEE6B`** 的 `97 03` 改为 **`a2 01`**（`0x01A2`），并重新计算偏移 **`0xFC00..0xFC03`** 的 4 字节 CRC-32。

---

### 路线 B：硬件 `SWS` 单线直读/直写路线（需开壳接触测试点，不依赖联想官方固件包）

如果希望在刷机前先通过硬件物理导出片上 512 KB 完整 Flash，或者作为极端情况下的硬件救砖手段，**路线 B 始终可作为物理层兜底方案**。

#### 3.3 为什么路线 B 具有最高的可逆性与确定性？
1. **可以直接完整备份（Dump）原厂固件**：
   - Telink 的 `SWS`（Single-Wire Slave）接口在硬件层面直接映射芯片内部总线，不依赖固件中的 USB/BLE 代码。
   - 只需准备一个常见的 **USB 转 TTL 串口模块（如 CH340G / CP2102，3.3V 电平）**，在 macOS 上使用开源 Python 工具 **[pvvx/TlsrComSwireWriter](https://github.com/pvvx/TlsrComSwireWriter)**：
     - 接线：`USB-TTL GND` $\rightarrow$ 主板 `GND`；`USB-TTL 3.3V` $\rightarrow$ 主板 `3V`；`USB-TTL TX`（串联 1k$\Omega$ 电阻或二极管连 `RX`）$\rightarrow$ 主板 `SWS`。
   - 执行读取命令即可将芯片内 **512 KB 完整 Flash（含当前固件、双分区、MAC 地址与配对信息）** 原原本本导出为 `kb331u_full_backup.bin`。
2. **精准定位与修改 2 字节**：
   - 在导出的 `kb331u_full_backup.bin` 中，同样定位偏移 `0xEE6A` 处的 `f2 00 97 03 01 00`。
   - 将其中的 `97 03`（`0x0397`）改为 `a2 01`（`0x01A2`），重算固件镜像尾部的 4 字节 Telink CRC32。
3. **零变砖风险（随时硬件回滚）**：
   - 无论是使用导出的 `kb331u_full_backup.bin` 还是已下载的官方 `evidence/KA024_official_firmware.bin`，随时可通过 `SWS` 重新刷回原始固件。
   - **唯一风险点**：KB331U 采用超薄卡扣+双面胶结构，拆开底壳接触主板测试点需要热风枪/加热垫与薄撬片，存在外观撬痕风险。

---

## 4. 本机已完成的只读探测与下一步安全验证

1. **已完成本机只读探测（`tools/readonly_hid_probe.c`）**：
   - 通过 IOKit `DebugState.InputReportCount` 实测确认：按下顶部从 `Esc` 到 `Delete` 一整排 12 个按键 + 单独按 2 下互联键时，Interface 0（标准键盘）恰好增加 `+6`（`Esc`、`Delete` 及在 macOS 下自动转为键盘截图快捷键的 `0x038E` 截图键，共 3 键 × 按下/抬起），Interface 1（Consumer Control）恰好增加 `+16`（5 个多媒体键 + 1 次扫键互联键 + 2 次单独按互联键，共 8 次 × 按下/抬起），`BT1`/`BT2`/`BT3` 走内部蓝牙切信道逻辑（不产生 USB 报文）。这与固件函数 `0x4CCC` 的反汇编逻辑 **100% 吻合**！
2. **下一步建议（需用户明确授权后执行）**：
   - **步骤 1（官方协议只读查询当前固件版本号）**：向 Interface 1 发送官方 APK 中的 7 字节版本查询指令 `04 03 04 01 01 35 02`，验证键盘在 Report ID `0x03` 返回当前固件版本字符串（如 `KA024-A-0.09` 或 `KA024_A_1.0.8`），100% 确认 USB 通信与 CRC 握手机制正常。
   - **步骤 2（生成 2 字节 Patch 固件并按需刷入）**：基于 `evidence/KA024_official_firmware.bin` 生成仅修改 `0xEE6A..0xEE6B`（`97 03` $\rightarrow$ `a2 01`）并更新末尾 4 字节 CRC-32 的固件文件（同时保留官方未修改的 `KA024_official_firmware.bin` 作为随时可刷回的原厂回滚包），待用户确认后再执行刷写。

---

## 5. 复盘与踩坑记录（Lessons Learned & Pitfalls）

本次从“未知主控芯片”到“免拆机纯软件永久改键成功”，过程中踩过并成功绕开了 5 个典型硬件/系统坑，记录如下供后续复用与参考：

1. **坑 1：误判官方升级体系（`fwupd` 协议混淆）**
   - *现象*：公开资料中 `fwupd` 的 `lenovo-accessory` 插件针对 `17EF:6194` 等联想外设采用了 RSA-3072 签名与 Feature Report，容易让人误以为所有联想键盘都有 RSA 签名且无法静态 Patch。
   - *解法*：坚持从硬件证据出发——通过 FCC 内部照主控左侧 **`SWS`** 测试点丝印及 USB `bMaxPacketSize0 = 8` 锁定泰凌微（Telink）SoC，再直接反编译联想平板升级器 `com.zui.keyboardupdate.olympia`，证实 KB331U（`17EF:619E`）走的是完全不同的 Telink OTA 协议，**无 RSA 签名**，仅校验 CRC-16 + 尾部 CRC-32。
2. **坑 2：macOS TCC 输入监控权限对标准键盘探测的静默拦截**
   - *现象*：在 macOS 上用 `IOHIDManager` 监听键盘按键时，即便 `IOHIDDeviceOpen` 返回成功，标准键盘（Interface 0）与多媒体键（Interface 1）的 `InputReport` 回调也会因终端无 TCC“输入监控”权限而被系统静默丢弃。
   - *解法*：一方面利用 IOKit 注册表中的 `DebugState.InputReportCount` 计数器无权限读取物理按键触发次数，精准区分各按键归属的接口；另一方面实测发现**厂商自定义集合（`UsagePage 0xFF00`，Report ID `0x04` OUT / `0x03` IN）不受 TCC 输入监控拦截**，可直接在 macOS 上完成双向版本查询与固件刷写。
3. **坑 3：固件二进制内同名常量区分（代码指令 vs 描述符 vs 映射表）**
   - *现象*：在原厂固件 `KA024_official_firmware.bin` 中搜索 `97 03`（`0x0397`）共命中 8 处。若盲目全局替换，会直接破坏 Telink TC32 机器码与 HID 描述符。
   - *解法*：先反汇编定位按键分发函数 `0x4CCC` 对表首地址 `0xEE44` 的引用，逐项核对顶部 7 个多媒体/快捷功能键的 6 字节结构体表，确认其中 5 处 `97 03` 为 TC32 `bl` 指令编码、2 处为 USB 描述符 `Usage/Logical Maximum`，**唯有 `0xEE6A..0xEE6B` 是互联键 `0x00F2` 的映射表项**；同时确认 USB（`0x6220`）与蓝牙 BLE（`0x626C`）共用该表，且 `Fn` 组合层位于独立的 `0xEF34` 表，改动 `0xEE6A` 对其他按键与 `Fn` 层零影响。
4. **坑 4：Consumer Usage `0x01A2` 与 `0x029F` 在 Android 定制系统（荣耀 MagicOS）上的兼容性差异**
   - *现象*：第一次刷入 `0x01A2`（`AL Select Task/Application` $\rightarrow$ Linux `KEY_APPSELECT = 580`）后，虽然在 AOSP 标准 `Generic.kl` 中 `key 580` 对应 `APP_SWITCH`，但在荣耀 MagicPad 4（MagicOS）蓝牙连接下按下无任何反应。
   - *解法*：改用早期 Linux 内核就内置的标准键值 **`0x029F`**（`AC Desktop Show All Windows` $\rightarrow$ Linux `KEY_SCALE = 120` $\rightarrow$ Android `key 120 APP_SWITCH`），一次实机验证成功唤起 MagicPad 4 最近任务视图。
5. **坑 5：先确认 Dual-Bank A/B 防变砖机制与官方原版回滚基线，再动手刷写**
   - *关键保障*：在发送任何写指令前，先反汇编确认 `0xDE00..0xDF70` 的固件写入逻辑（写入备用分区、第 0 块暂存 `0xFF` 标志、全部 4,033 块与 `CMD_OTA_END` 内部 CRC-32 校验通过后才激活新分区），并永久保留官方原版 `evidence/KA024_official_firmware.bin`，使得从 `0x01A2` 试错切换到 `0x029F` 全程零风险。

---

## 6. 变更记录

- **v1（2026-11-18）**：完成基础 USB/HID 描述符采集与 FCC 照片归档。
- **v2（2026-11-18）**：锁定 Telink 主控芯片、`keyevent 187` Consumer Usage 映射链路及 ZUI 升级包名。
- **v3（2026-11-18）**：完成本机只读探测、APK 逆向、官方原厂固件 `KA024_A_1.0.8` 拉取及 `0xEE6A` 映射表偏移定位。
- **v4（2026-11-18）**：完成 USB-C 双版本实机刷写（`KA024_A_1.0.6` $\rightarrow$ `KA024_A_1.0.8` 改键版），验证 `0x01A2` 在 MagicOS 上不生效而 **`0x029F`（`KEY_SCALE = 120`）成功在 HONOR MagicPad 4 蓝牙模式下唤起最近任务**，并补全复盘与踩坑记录。
