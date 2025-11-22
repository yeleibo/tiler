cmake_minimum_required(VERSION 3.16)
project(MapTiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt设置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# 指定Qt 5.15.2路径（根据你的安装路径修改）
# 例如: set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/msvc2019_64")
# set(CMAKE_PREFIX_PATH "你的Qt安装路径")

# 查找Qt5组件
find_package(Qt5 5.15.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql
)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TileMap.cpp
    src/Tile.cpp
    src/Layer.cpp
    src/Task.cpp
    src/Config.cpp
    src/Utils.cpp
    src/GeoJsonReader.cpp
)

# 头文件
set(HEADERS
    include/MainWindow.h
    include/TileMap.h
    include/Tile.h
    include/Layer.h
    include/Task.h
    include/Config.h
    include/Utils.h
    include/GeoJsonReader.h
)

# UI文件
set(UI_FILES
    ui/MainWindow.ui
)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# 链接Qt库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
)

# Windows平台特殊设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )
    # 添加MSVC UTF-8编码支持，解决中文字符编译问题
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
