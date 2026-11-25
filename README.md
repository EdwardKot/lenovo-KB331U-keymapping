# Lenovo KB331U (17EF:619E) 固件级永久改键与 WebHID 改键工具

> 针对联想小新无线键盘 Air（KB331U / 内部代号 `KA024` / Telink QFN48 SoC）的免拆机固件级永久改键研究与 WebHID 可视化改键工具。  
> 已实机验证：将右上角私有 **Device Interconnection（设备互联）键**（`0x0397`）永久重映射为标准 HID Consumer Usage **`0x029F`**（`AC Desktop Show All Windows` / Linux `KEY_SCALE = 120`），在 **HONOR MagicPad 4（MagicOS 蓝牙模式、无 Root、无后台 App）** 下原生触发最近任务视图（`keyevent 187`），断电重配对永久有效。

---

## 快速开始：WebHID 网页改键工具

无需安装驱动或编译程序，直接通过 **Chrome / Edge** 经 **USB-C 有线** 或 **蓝牙无线（Web Bluetooth）** 修改整把键盘（75 键，美式布局）的键位，或一键恢复联想原厂默认固件：

```bash
./web/serve.sh
```

随后在浏览器打开 `http://localhost:8765`，连接键盘 USB-C 或蓝牙即可：
1. 读取当前键盘固件标签（原厂 `KA024_A_1.0.6` / `KA024_A_1.0.8`，或自定义方案标签；蓝牙连接同时显示电量）；
2. 在全键盘图上点任意键（BT1–3 锁定），改成任意普通键、多媒体/系统快捷功能（**自动支持 7→14 槽扩容**）、**一键双键组合快捷键（硬件宏）**或禁用；
3. 可选开启 **「长按变 Fn 层」**（例如长按 `CapsLock` + `IJKL` 变方向键、`1–=` 变 `F1–F12`，轻点 `CapsLock` 变 `Esc`，支持任意松手顺序防卡键）与 **「双击快捷键」**（例如连续轻点两下空格左侧 `Alt/cmd` 自动触发 `Meta/option + S` 唤出安卓平板搜索，平时按住做组合键依然 0 延迟无冲突）；
4. 支持永久自定义**方案标签、蓝牙广播名与 USB 设备名**，并可导出/导入 `.json` 方案或从 `.bin` 读回；
5. 自动通过逐字节白名单核验并重算 Telink CRC-32 与双层 CRC-16，通过 USB-C（~15s）或蓝牙 BLE OTA（~30–90s）安全刷入备用分区（Dual-Bank A/B 防变砖）。

---

## 仓库目录结构

- `web/` — 纯静态 WebHID 改键工具（[web/index.html](web/index.html)、[web/serve.sh](web/serve.sh)、内置官方原厂基线 `KA024_official_firmware.bin`）
- `tools/` — macOS 原生研究与刷写工具源码：
  - `readonly_hid_probe.c` — IOKit 只读 HID 探测工具
  - `query_kb_version.c` — USB Interface 1 (`0xFF00`) 只读版本查询工具
  - `flash_kb_firmware.c` — USB-C 全量分块刷写工具
  - `query_lenovo_ota.py` — 联想官方 OTA 服务器固件查询脚本
  - `parse_hid_descriptor.py` — HID Report Descriptor 解析脚本
- `evidence/` — 硬件与协议取证归档（官方原厂固件 `KA024_official_firmware.bin`、改键固件、FCC 资料、`ioreg` 与 HID 描述符等）
- [REPORT.md](REPORT.md) — 完整深度研究报告（v4，含芯片溯源、协议逆向、键表定位与复盘踩坑记录）
