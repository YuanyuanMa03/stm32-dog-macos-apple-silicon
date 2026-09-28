# 个性化语音：SU-03T1 与可替代方案

## 继续使用现有 SU-03T1

原资料包中的 `SU-03T小狗 .json` 是小狗语音项目配置，包含唤醒词、命令词、动作和发声设置；同目录 `jx_su_03t_release_update.bin` 是当前已生成的 **串口升级固件**。如需修改个性化词句，按资料中的“导入步骤”图，将 JSON 导入[智能公元平台](http://www.smartpi.cn/)，在那里修改、生成新固件，再用本仓库的 Mac C 烧录器写入新生成的 `_update.bin`。新固件与旧文件哈希不同，运行时不要继续使用旧哈希作为 `--sha256` 参数；先对新文件运行 `shasum -a 256` 并检查来源。

**不要把原始 JSON 上传到公开 Git 仓库。** 当前导出文件含平台标识、访问字段和下载链接；公开仓库只记录字段含义与操作方法。厂商[技术开发手册](https://docs.aimachip.com/zh-cn/latest/_static/document/SU-03T/SU-03T%E6%8A%80%E6%9C%AF%E6%89%8B%E5%86%8Cv1.2.pdf)说明可下载的本地 SDK 只提供基础功能，完整功能要由平台编译生成。C 烧录器负责把已生成的固件通过 CH340 写入模块，它不能从 JSON 自行编译出 SU-03T1 的完整语音识别固件。

当前模块还没有发出串口升级握手 `CCC`。无论使用 Python、C 还是 Windows 软件，**B6/B7 串口升级都需要模块先进入相应 boot 并发出握手**。因此现阶段不能宣称个性化语音已写入。先排查供电、3.3V UART 电平、线材及是否预装串口升级 boot；厂商[常见问题](https://docs.aimachip.com/zh-cn/latest/file/question/SU03T.html)和本仓库[实测记录](voice-flash.md)列出了依据。

## 若要减少对厂商语音平台的依赖

[Espressif ESP-SR](https://github.com/espressif/esp-sr) 在 ESP32-S3 等芯片上支持离线中文命令词，官方提供 [ESP-IDF 的 macOS 编译与烧录流程](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html)。这是**更换语音硬件**的路线，不能把 ESP32-S3 固件写进 SU-03T1，也不能直接插入现有 SU-03T1 插座。需要另选带麦克风的 ESP32-S3 音频板，并在新板与 STM32 之间实现与桌宠指令兼容的 3.3V UART 接口。ESP-SR 仓库虽公开源码，但其中有预编译库/模型；具体许可和可再分发性应按其仓库与模型逐一确认，不能称为完全开源的语音模型。
