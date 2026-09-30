# 编译 Windows 测试版

## 最省事：GitHub Actions

本目录已经包含 `.github/workflows/build-windows-lvtest.yml`。

1. 把整个目录上传到一个 GitHub 仓库。
2. 打开仓库 `Actions`。
3. 选择 `build-windows-lvtest`。
4. 点 `Run workflow`。
5. 完成后下载 Artifact：`pktriggercord-lvtest-windows`。
6. 里面就是 `pktriggercord-lvtest.exe`。

## 本机 MSYS2 / MinGW

如果已安装 i686 MinGW 交叉编译器，可在源码目录执行：

```bash
ARCH=Win32 make lvtest
```

生成：

```text
pktriggercord-lvtest.exe
```

这个实验程序只有命令行界面，不依赖 GTK GUI DLL。
