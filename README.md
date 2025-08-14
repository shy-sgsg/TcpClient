# TcpClient

一个基于Qt框架的TCP客户端应用，用于监控指定目录下的文件变化并自动将新文件通过TCP协议发送到服务器。

## 功能介绍

- 监控指定目录，当有新文件出现时自动加入发送队列
- 按队列顺序发送文件，支持文件传输进度显示
- 实时显示当前传输速度
- 支持文件传输失败重试机制
- 完善的日志记录功能
- 支持开始/停止监控操作

## 技术栈

- C++17
- Qt 5/6 (Widgets, Network模块)
- CMake 3.16+

## 项目结构

```
TcpClient/
├── CMakeLists.txt          # 项目构建配置
├── main.cpp                # 程序入口
├── mainwindow.h/.cpp       # 主窗口类
├── mainwindow.ui           # 主窗口UI设计
├── logmanager.h/.cpp       # 日志管理类
├── filesenderworker.h/.cpp # 文件发送工作类
└── .gitignore              # Git忽略文件配置
```

## 编译与运行

### 前提条件

- 安装Qt 5.15+ 或 Qt 6.0+ 开发环境
- 安装CMake 3.16+
- 支持C++17的编译器(GCC 8+, Clang 7+, MSVC 2017+)

### 编译步骤

1. 克隆仓库
   ```bash
   git clone <仓库地址>
   cd TcpClient
   ```

2. 创建构建目录并编译
   ```bash
   mkdir build
   cd build
   cmake ..
   make  # 或在Windows上使用cmake --build .
   ```

3. 运行生成的可执行文件

## 使用说明

1. 启动程序后，点击"开始监控"按钮启动目录监控
2. 程序会自动监控预设目录（默认：E:/AIR/小长ISAR/实时数据回传/data）
3. 当监控目录中有新文件出现时，程序会自动将其加入发送队列并依次发送
4. 传输进度和速度会实时显示在界面上
5. 点击"停止监控"按钮可停止目录监控

## 配置说明

- 监控目录可在`mainwindow.cpp`的`on_pushButton_clicked`函数中修改
- 最大重试次数定义在`mainwindow.cpp`中的`MAX_RETRIES`常量（默认：5次）
- 重试延迟定义在`mainwindow.cpp`中的`RETRY_DELAY_MS`常量（默认：2000毫秒）
- 服务器地址和端口在`filesenderworker.cpp`的`process`函数中设置（默认：localhost:65432）

## 注意事项

- 确保监控目录存在且程序有读写权限
- 确保服务器端正常运行并监听正确的端口
- 大文件传输可能需要较长时间，请耐心等待

## 日志记录

程序会记录所有操作日志，包括文件发送状态、错误信息等，可通过界面查看日志输出。