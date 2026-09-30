# K-3 II HDMI Live View + USB 被动取 PEF：实验版

目的：验证 **K-3 II 保持 LV/HDMI 实时预览时，USB/MSC 是否仍可只读取拍照后的 RAW buffer**。

这个版本刻意绕过原版 CLI 在 `Connected` 之后会做的大量 `pslr_get_status()`、`read_settings`、文件格式设置等操作。

## 第一次测试（推荐，最少 USB 通信）

1. 相机 `USB Connection = MSC`。
2. 相机接 HDMI，按 `LV`，确认显示器实时画面正常。
3. USB 接电脑；不要打开原版 pkTriggerCord GUI。
4. 运行：

```powershell
.\pktriggercord-lvtest.exe --device F --output lvtest.pef --debug
```

5. 程序显示 `Connected: K-3II` 后，用手机红外遥控 **拍 1 张**。
6. 等 HDMI 画面完成快门黑屏并恢复 Live View 后，在命令行按 Enter。
7. 程序只尝试直接选择 buffer 0 并下载 PEF，不再读取完整 status/settings。
8. 检查 `lvtest.pef`：
   - 能否正常打开；
   - HDMI Live View 是否恢复且相机没有死机。

### 注意

- 第一次不要加 `--delete`；程序不会删除相机 buffer，减少额外命令。
- 测完后如果相机状态异常，退出程序、拔 USB、相机关机再开即可。
- 这是协议实验，不修改相机固件，不向相机刷写任何永久内容。

## 若第一次成功

第二次可测试轻量自动检测：

```powershell
.\pktriggercord-lvtest.exe --device F --output lvtest2.pef --poll --debug
```

`--poll` 只轮询 `0x02/0x00` buffer mask，不轮询完整 `0x00/0x08` status。

若 `--poll` 也稳定，下一步即可扩展为：红外拍照 → 自动检测 buffer → 下载 PEF → ExifTool 抽 JPG → 删除 PEF → 照片墙。

## 编译 Windows EXE

项目原 Makefile 已加入 `lvtest` target。使用原项目推荐的 MinGW 交叉编译环境：

```bash
ARCH=Win32 make lvtest
```

输出：`pktriggercord-lvtest.exe`
