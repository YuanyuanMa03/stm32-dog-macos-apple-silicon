# 在 Apple Silicon Mac 烧录 STM32

本页对应原项目 **V1.2.1** 的 `STM32F103C8T6` 主控。本次在 arm64 / macOS 15.7.1 上用 CH340 和 `stm32flash 0.7` 完成写入与校验；设备返回 `Device ID: 0x0410 (STM32F10xxx Medium-density)`。`stm32flash` 可自动识别 Intel HEX，[项目说明](https://sourceforge.net/p/stm32flash/wiki/Home/)也列出读取、写入和校验功能。

## 1. 驱动与端口

先插 CH340，在终端执行：

```bash
ls /dev/cu.* | grep -E 'wch|usbserial|usbmodem'
brew install stm32flash
```

本次端口是 `/dev/cu.wchusbserial1440`；其他人的后缀会不同。Mac 没出现 CH340 端口时，从 [WCH 官方 macOS 驱动](https://github.com/WCHSoftGroup/ch34xser_macos)按当前系统说明安装，再拔插确认。**出现可打开的串口且已读到 STM32 ID 时，不应再把超时归因于驱动。**

## 2. 接线与引导模式

先断电；CH340 **TX 接板上 MCU RX、RX 接 MCU TX、GND 共地**。V1.2.1 的丝印与旧版不同，最终以[原作者 V1.2.1 接线图和视频](https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet)核对。把 STM32 最小系统板上的 **BOOT0 拨到 1**，再复位或重新上电。连接时不要把语音模块的 B6/B7 当成 STM32 下载口。

确认设备响应（把 `PORT` 改成你的端口）：

```bash
PORT=/dev/cu.wchusbserial1440
stm32flash "$PORT"
```

若出现 `Failed to init device, timeout`，按顺序查：BOOT0 是否为 1、拨动后是否复位/重新上电、TX/RX 是否交叉、GND 是否共地、接头/杜邦线是否接触良好。本次成功前，恰好通过调换 TX/RX 并将 BOOT0 置 1 后复位解决了超时。

## 3. 备份、写入、校验

取得原作者资料包内 **V1.2.1** 的 `Project.hex`，先算 SHA-256 确认文件，再执行：

```bash
shasum -a 256 '/你的资料目录/Project.hex'
mkdir -p backups
stm32flash -r backups/stm32-before.bin "$PORT"
stm32flash -w '/你的资料目录/Project.hex' -v "$PORT"
```

每次启动一个新的 `stm32flash` 命令前，若设备不再应答，保持 BOOT0=1 并按一次 RESET/重新上电，让系统引导程序重新等待握手。本次使用的 `Project.hex` SHA-256 为 `69dff6327e093cabf060d7231bc4feafa4d4c5a8592eab1d487f086dcf9b8a41`，备份为 65,536 字节。终端出现校验成功才算完成写入。备份可能包含之前的个人固件，请留在本地，不要上传公开仓库。

## 4. 运行

烧录后把 **BOOT0 拨回 0**，再复位/重新上电；否则 MCU 仍会进入系统引导程序。此后再按原作者接线装配 OLED、舵机、蓝牙与语音模块。STM32 成功不代表语音模块已经烧录成功。
