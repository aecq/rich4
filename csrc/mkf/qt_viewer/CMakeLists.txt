cmake_minimum_required(VERSION 3.16)
project(MkfViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关键配置：自动运行 MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# 添加源文件
add_executable(MkfViewer main.cpp mkf_decompress.c)

# 链接 Qt 库
target_link_libraries(MkfViewer Qt6::Core Qt6::Gui Qt6::Widgets)
