# Lenovo KB331U (17EF:619E) 固件级永久改键与 WebHID 改键工具

> 针对联想小新无线键盘 Air（KB331U / 内部代号 `KA024` / Telink QFN48 SoC）的免拆机固件级永久改键研究与 WebHID 可视化改键工具。  
> 已实机验证：将右上角私有 **Device Interconnection（设备互联）键**（`0x0397`）永久重映射为标准 HID Consumer Usage **`0x029F`**（`AC Desktop Show All Windows` / Linux `KEY_SCALE = 120`），在 **HONOR MagicPad 4（MagicOS 蓝牙模式、无 Root、无后台 App）** 下原生触发最近任务视图（`keyevent 187`），断电重配对永久有效。

---

## 快速开始：WebHID 网页改键工具

无需安装驱动或编译程序，直接通过桌面版 **Chrome / Edge** 经 USB-C 修改键盘顶部 7 个多媒体/快捷功能键，或一键恢复联想原厂默认固件：

```bash
./web/serve.sh
```

随后在浏览器打开 `http://localhost:8765`，连接键盘 USB-C 接口即可：
1. 读取当前键盘固件版本（如 `KA024_A_1.0.6` / `KA024_A_1.0.8`）；
2. 可视化修改顶部 7 键（静音、音量-/+、亮度-/+、截图、设备互联），内置 MagicOS/Android 推荐键值与自定义 16-bit Hex 输入；
3. 自动重算 Telink CRC-32 与双层 CRC-16，通过 USB-C 安全刷入备用分区（Dual-Bank A/B 防变砖）或导出 `.bin` 备份。

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
