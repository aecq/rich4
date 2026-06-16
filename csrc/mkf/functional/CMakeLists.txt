cmake_minimum_required(VERSION 3.16)
project(MkfViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Core5Compat Gui Widgets Multimedia REQUIRED)

set(SOURCES
    src/core/utils/cache.cpp
    src/core/utils/resource_model.cpp
    src/core/utils/audio_player.cpp
    src/core/utils/flic_decoder.cpp
    src/gui/graphics_text_window.cpp
    src/gui/map_graphics_view.cpp
    src/gui/image_player_widget.cpp
    src/gui/main_window.cpp
    src/main.cpp
    ../mkf_decompress.c
)

set(HEADERS
    include/core/utils/cache.h
    include/core/utils/resource_model.h
    include/core/utils/audio_player.h
    include/core/utils/flic.h
    include/core/utils/flic_details.h
    include/gui/graphics_text_window.h
    include/gui/map_graphics_view.h
    include/gui/image_player_widget.h
    include/gui/main_window.h
    ../mkf_decompress.h
)

add_executable(MkfViewer ${SOURCES} ${HEADERS})

target_include_directories(MkfViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MkfViewer Qt6::Core Qt6::Core5Compat Qt6::Gui Qt6::Widgets Qt6::Multimedia)