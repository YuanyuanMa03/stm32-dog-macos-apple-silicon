# STM32 桌宠狗：Apple Silicon Mac 复刻记录

面向 **Sngels_wyh V1.2.1 电路板 + STM32F103C8T6 + SU-03T1**。这里整理 Mac M 芯片上采购、焊接、STM32 串口烧录、独立语音模块升级、装配与验收的顺序。原项目和资料见 [Sngels_wyh 的立创页面](https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet)，请先核对板子版本；V1.0/V1.1 的接线和固件不能直接照搬。

**实测进度（2026-09-28）：**在 arm64 / macOS 15.7.1 上，CH340 可识别为 `/dev/cu.wchusbserial1440`，`stm32flash 0.7` 识别到 `0x0410`，V1.2.1 的 STM32 固件写入、校验成功。SU-03T1 目前是离开桌宠板单独接 CH340；串口能打开，但两次上电均未收到升级握手，**语音固件尚未写入，整机验收尚未完成**。这是可复现记录，不把未完成步骤标成成功。

## 路线

1. [准备物料与版本](docs/materials.md)：V1.2.1 主板、五个 SG90、STM32F103C8T6、SU-03T1 等。
2. [在 Mac 烧录 STM32](docs/stm32-flash.md)：安装/确认 CH340 驱动，BOOT0 进入系统引导，先备份后写入并校验。
3. [单独烧录 SU-03T1](docs/voice-flash.md)：模块不插主板，B6/B7 为升级口，5V 供电与 3.3V UART 信号须分开确认。
4. [装配与验收](docs/assembly.md)：恢复 BOOT0、安装语音模块、检查舵机/OLED/蓝牙/语音联动。

## 取得原项目文件

从[原作者项目页](https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet)底部的资料链接取得对应 **V1.2.1** 资料。本仓库不镜像原作者的固件、源代码、PCB、模型和第三方 Windows 工具；原因与许可范围见 [NOTICE](NOTICE.md)。

本次实测使用的原资料相对路径和 SHA-256：

| 用途 | 原资料中的文件 | SHA-256 |
| --- | --- | --- |
| STM32 串口写入 | `桌宠代码/适配1.2.1版本电路板的代码/桌宠代码hex文件，直接烧录，不用再用编译器了/Project.hex` | `69dff6327e093cabf060d7231bc4feafa4d4c5a8592eab1d487f086dcf9b8a41` |
| SU-03T1 串口升级 | `语音固件/我的语音固件(SU-03T1)/jx_firm/jx_su_03t_release_update.bin` | `76ef7546435ba784510567143d8d4ab185834d6124d94998e43a8dc6429c3d1c` |

如果原作者更新文件，哈希可能变化；先核实版本及文件来源，再决定使用新文件。SU-03T1 的非 `_update.bin` 文件供专用烧录器使用，**不能**交给 B6/B7 串口升级。

## 快速开始

```bash
brew install stm32flash
ls /dev/cu.* | grep -E 'wch|usbserial|usbmodem'
stm32flash /dev/cu.wchusbserial1440
```

上面的设备名只是本次实例，以你自己 Mac 显示的端口为准。BOOT0、供电、TX/RX 接好并复位后，再运行识别命令。完整的备份与写入顺序见 [STM32 烧录步骤](docs/stm32-flash.md)。

语音模块请先阅读 [独立烧录步骤](docs/voice-flash.md)。这一路仍在硬件验证中；不要因为 STM32 烧录成功就认为语音模块也已烧好。

## 项目来源

本仓库只授权自己写的指南和工具，MIT 许可。电路与桌宠功能来自 [Sngels_wyh 原项目](https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet)；CH340 驱动见 [WCH 官方仓库](https://github.com/WCHSoftGroup/ch34xser_macos)；SU-03T1 的引脚/电气信息见[机芯智能官方资料](https://docs.aimachip.com/zh-cn/latest/file/offlineVoice/SU03T.html)。
