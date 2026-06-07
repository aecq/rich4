cmake_minimum_required(VERSION 3.16)
project(MkfViewer3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Core5Compat Gui Widgets Multimedia REQUIRED)

set(SOURCES
    src/core/utils/Cache.cpp
    src/core/utils/MediaPlayer.cpp
    src/gui/graphics_text_window.cpp
    src/gui/main_window.cpp
    src/main.cpp
    ../mkf_decompress.c
)

set(HEADERS
    include/core/utils/Cache.h
    include/core/utils/MediaPlayer.h
    include/gui/graphics_text_window.h
    include/gui/main_window.h
    ../mkf_decompress.h
)

add_executable(MkfViewer3 ${SOURCES} ${HEADERS})

target_include_directories(MkfViewer3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MkfViewer3 Qt6::Core Qt6::Core5Compat Qt6::Gui Qt6::Widgets Qt6::Multimedia)