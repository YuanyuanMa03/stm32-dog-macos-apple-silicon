# SU-03T1 脱离主板、单独串口升级

这里的“独立/离线烧录”指 **SU-03T1 不插在桌宠主板上**，用 CH340、独立供电和 Mac 直接升级模块。语音识别本身也离线运行；这与厂家称为“升级狗”的 **脱机烧录器** 是不同设备。厂家资料区分：B6/B7 为串口升级、B0/B1 为调试器、B2/B3 为升级狗。[官方引脚说明](https://docs.aimachip.com/zh-cn/latest/file/offlineVoice/SU03T.html)

**状态：尚未完成硬件写入。** CH340 在 Mac 上可打开；按厂商接法以及按原项目《常见问题处理方法》交换 TX/RX 后重新上电，均未收到升级握手，上传器发送 0 个分组。再次试验时 CH340 端口变成 `/dev/cu.wchusbserial1430`，固件暂存到纯英文路径，并在 921600 8N1 监听期间两次给模块上电；只收到 1 个 `00` 字节。随后把 CH340 TX 保持低电平，让模块 VCC 两次完整断电再上电，收到 2 个 `00` 字节。最后用 Mac C 烧录器打开同一端口，确认固件 SHA-256 后再次上电，只收到 1 个 `00`。所有尝试均未收到 `CCC`，依然未写入固件。当前供电来自 CH340 的 5V 引脚，尚未用独立稳定 5V 复测。CH340 的 UART 信号电平也仍需确认。本文列出已确认的资料与可运行的 Mac C 上传工具，不把它写成已验证成功。

## 接线

| CH340 / 电源 | SU-03T1 | 说明 |
| --- | --- | --- |
| CH340 TX | B6（UART1_RX） | **TX 信号必须为 3.3V 逻辑电平** |
| CH340 RX | B7（UART1_TX） | 接收模块 3.3V 信号 |
| CH340 GND、5V 电源 GND | 模块 GND | 三者共地 |
| 稳定的独立 5V 电源 | 模块 VCC | 不用 CH340 的 5V 引脚给模块供电；供电 5V 与串口逻辑 3.3V 是两件事 |

以上对应原资料中的《USB Update Tool User Guide》第 2 页图 3：CH340 到模块只画了 TXD、RXD、GND，模块的电源另有红黑线。[厂商规格](https://docs.aimachip.com/zh-cn/latest/file/offlineVoice/SU03T.html)写明 5V 供电与 3.3V UART。如果 CH340 仅把电源选到 3.3V，却不能确认 TX 信号电平，先测量转换器 TX 空闲时对 GND 的电压（测量时与模块 B6 断开），不要猜测。

## 固件与启动顺序

从原项目资料包选择 `语音固件/我的语音固件(SU-03T1)/jx_firm/jx_su_03t_release_update.bin`；本次文件 SHA-256 为 `76ef7546435ba784510567143d8d4ab185834d6124d94998e43a8dc6429c3d1c`。文件名带 `_update.bin` 才用于 B6/B7 串口升级；`jx_su_03t_release.bin` 是专用烧录器固件。[机芯智能开发手册](https://docs.aimachip.com/zh-cn/latest/_static/document/SU-03T/SU-03T%E6%8A%80%E6%9C%AF%E6%89%8B%E5%86%8Cv1.2.pdf)

厂家顺序是 **先打开烧录工具并进入等待设备，再给模块上电**。原项目所附 Windows `UniOneUpdateTool.exe` 的历史日志显示 `921600 8N1`、收到连续 `C` 后开始 XMODEM 传输；本仓库自行编写的 [Mac C 烧录器](../tools/su03t-flash.c) 按该信息实现 XMODEM-1K/CRC。它通过 C 模拟接收器测试，但还没有在实际 SU-03T1 上完成写入与开机验收。

仅在确认 TX 是 3.3V 后，在 Mac 上执行：

```bash
make
make test
ls /dev/cu.wchusbserial*
./build/su03t-flash \
  --port /dev/cu.wchusbserial1430 \
  --firmware '/你的资料目录/jx_su_03t_release_update.bin' \
  --sha256 76ef7546435ba784510567143d8d4ab185834d6124d94998e43a8dc6429c3d1c
```

脚本显示 `READY` 时，才给语音模块接通 5V。没有收到 `CCC` 就不会发送任何固件字节；传输结束必须收到 EOT 的 ACK，并在模块重新启动后测试语音功能，才能认为模块可用。
每次重新插拔 CH340 后，先用 `ls /dev/cu.wchusbserial*` 查看当前串口名；后缀可能变化。Mac C 程序支持中文路径，但原项目《常见问题处理方法》还建议把固件放在纯英文路径排查问题；本次已经照做，握手仍未出现。原资料中的 `unionedebug.ini` 注释是 GB18030 编码，可用 `iconv -f GB18030 -t UTF-8 unionedebug.ini` 查看；固件 `.bin` 是二进制，不能对它做文字转码。

如果仍无握手，按[厂家故障排查](https://docs.aimachip.com/zh-cn/latest/file/question/SU03T.html)核对独立稳定 5V、B6/B7/GND、线材、串口工具；有些模块没有预装可串口升级的 boot，需先用蜂鸟 M 调试器或升级狗写入专用烧录器版本。[厂商开发手册](https://docs.aimachip.com/zh-cn/latest/_static/document/SU-03T/SU-03T%E6%8A%80%E6%9C%AF%E6%89%8B%E5%86%8Cv1.2.pdf)

## 安装回 V1.2.1 桌宠主板

烧录用的 **B6/B7** 与 V1.2.1 主板上的语音控制串口不是同一对引脚。原作者说明 V1.2.1 兼容 SU-03T1/CI-03T，SU-03T1 与 MCU 通信应按该版固件配置 **B0 为 TX、B1 为 RX**，最终安装方向和丝印以[原作者 V1.2.1 教程](https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet)为准。先确认语音固件升级成功，再插回主板。
