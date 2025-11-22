# MapCloud Tiler - Qt版本

这是一个基于Qt 5.15.2的地图瓦片下载器，从原Go语言版本转换而来。

## 项目结构

```
qt/
├── CMakeLists.txt          # CMake构建配置
├── include/                # 头文件目录
│   ├── Config.h           # 配置管理类
│   ├── GeoJsonReader.h    # GeoJSON读取类
│   ├── Layer.h            # 图层类
│   ├── MainWindow.h       # 主窗口类
│   ├── Task.h             # 下载任务类
│   ├── Tile.h             # 瓦片类
│   ├── TileMap.h          # 瓦片地图类
│   └── Utils.h            # 工具类
├── src/                    # 源文件目录
│   ├── Config.cpp
│   ├── GeoJsonReader.cpp
│   ├── Layer.cpp
│   ├── main.cpp           # 程序入口
│   ├── MainWindow.cpp
│   ├── Task.cpp
│   ├── Tile.cpp
│   ├── TileMap.cpp
│   └── Utils.cpp
└── ui/                     # UI界面文件
    └── MainWindow.ui      # 主窗口UI设计
```

## 编译要求

- Qt 5.15.2
- CMake 3.16 或更高版本
- C++17 编译器
- SQLite支持（Qt SQL模块）

## 编译步骤

### Windows (使用 Visual Studio)

1. 打开 CMake GUI
2. 设置源代码路径为 `qt` 目录
3. 设置构建路径为 `qt/build`
4. 点击 Configure，选择 Visual Studio 版本
5. 设置 `CMAKE_PREFIX_PATH` 为你的 Qt 安装路径，例如：`C:/Qt/5.15.2/msvc2019_64`
6. 点击 Generate
7. 点击 Open Project 打开 Visual Studio
8. 在 Visual Studio 中编译项目

### Windows (使用命令行)

```bash
cd qt
mkdir build
cd build

# 设置Qt路径
set CMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64

# 配置
cmake ..

# 编译
cmake --build . --config Release
```

### Linux

```bash
cd qt
mkdir build
cd build

# 配置（确保Qt在PATH中或设置CMAKE_PREFIX_PATH）
cmake ..

# 编译
make -j4
```

## 使用说明

### 1. 准备配置文件

使用原项目的 `conf.toml` 配置文件，或创建新的配置文件。配置文件示例：

```toml
[app]
version = "v 0.1.0"
title = "MapCloud Tiler"

[log]
enable = true
file = "tiler.log"
level = "info"

[output]
format = "mbtiles"  # 或 "file"
directory = "output"

[task]
workers = 30
savepipe = 200
timedelay = 50
skipexisting = true
resume = true

[tm]
name = "wuhan-google-satelite"
min = 0
max = 20
format = "jpg"
schema = "xyz"
url = "http://mt0.google.com/vt/lyrs=s&x={x}&y={y}&z={z}"

[[lrs]]
min = 1
max = 20
geojson = "./geojson/wuhan.geojson"
```

### 获取geojson
#### 阿里
https://datav.aliyun.com/portal/school/atlas/area_selector
#### OpenStreetMap
https://nominatim.openstreetmap.org/search?q=%E6%AD%A6%E6%B1%89%E6%B1%9F%E6%B1%89%E5%8C%BA%E5%89%8D%E8%BF%9B%E8%A1%97%E9%81%93&polygon_geojson=1&format=jsonv2

把里面的q换成具体的关键字，结果检查"display_name":"前进街道, 江汉区, 武汉市, 湖北省, 中国" 看是否是自己需要的，然后将数据保存到geojson文件中
### 2. 运行程序

```bash
# Windows
bin\MapTiler.exe

# Linux
bin/MapTiler
```

### 3. 使用界面

1. **加载配置**：点击"浏览"选择配置文件，然后点击"加载配置"
2. **设置参数**：
   - 瓦片URL：输入瓦片服务的URL模板（支持 {x}, {y}, {z} 占位符）
   - 输出格式：选择 mbtiles 或 file
   - 缩放级别：设置最小和最大缩放级别
   - 并发数：设置同时下载的线程数（建议1-50）
   - 延时：设置请求间隔时间（毫秒）
   - 跳过已下载瓦片：启用后会跳过已存在的瓦片
   - 启用断点续传：支持中断后继续下载
3. **开始下载**：点击"开始下载"按钮
4. **控制下载**：可以使用"暂停"、"继续"、"停止"按钮控制下载过程

## 主要功能

### 已实现功能

- ✅ 从配置文件加载下载参数
- ✅ 支持多种瓦片URL格式（Google、OSM、Tianditu等）
- ✅ 支持输出为MBTiles或文件目录
- ✅ 多线程并发下载
- ✅ 下载进度显示
- ✅ 日志记录
- ✅ 跳过已下载的瓦片
- ✅ 断点续传功能
- ✅ GeoJSON边界支持
- ✅ 图形化界面



## 依赖库说明

### Qt模块
- Qt5::Core - 核心功能
- Qt5::Gui - GUI支持
- Qt5::Widgets - 窗口部件
- Qt5::Network - 网络下载
- Qt5::Sql - SQLite数据库

### 可选改进

如果需要更完整的功能，可以考虑添加以下第三方库：

1. **toml11** 或 **cpptoml** - 更完整的TOML解析支持
2. **nlohmann/json** - JSON解析（已在代码中使用Qt的JSON）
3. **GDAL** - 更强大的GeoJSON和坐标转换支持

## 常见问题

### Q: 如何设置Qt路径？

A: 在CMakeLists.txt中设置 `CMAKE_PREFIX_PATH`，或在环境变量中设置Qt路径。

### Q: 编译时找不到Qt模块？

A: 确保安装了Qt 5.15.2，并且包含了所有需要的模块（Core, Gui, Widgets, Network, Sql）。

### Q: 下载的瓦片存放在哪里？

A: 根据配置文件中的 `output.directory` 设置，默认在 `output` 目录。

### Q: 如何添加新的瓦片源？

A: 在配置文件的 `[tm]` 部分修改 `url` 字段，使用 {x}, {y}, {z} 作为坐标占位符。


