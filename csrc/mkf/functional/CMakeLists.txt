cmake_minimum_required(VERSION 3.16)
project(MkfViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

add_library(ma_lib STATIC
    src/core/utils/miniaudio.c
)

target_include_directories(ma_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include/core/utils
)

target_compile_definitions(ma_lib PRIVATE
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS
    MA_ENABLE_WASAPI
    MA_NO_DSOUND
    MA_NO_WINMM
    MA_NO_ALSA
    MA_NO_PULSEAUDIO
    MA_NO_JACK
    MA_NO_COREAUDIO
    MA_NO_AAUDIO
    MA_NO_OPENSL
    MA_NO_WEBAUDIO
    MA_NO_SNDIO
    MA_NO_AUDIO4
    MA_NO_OSS
    MA_NO_CUSTOM
    MA_NO_NULL
    MA_NO_MP3
    MA_NO_FLAC
    MA_USE_STDINT
)

set(SOURCES
    src/core/utils/cache.cpp
    src/core/utils/resource_model.cpp
    src/core/utils/big5hkscs_table.cpp
    src/core/utils/flic_decoder.cpp
    src/gui/graphics_text_window.cpp
    src/gui/map_graphics_view.cpp
    src/gui/map_panel_widget.cpp
    src/gui/image_player_widget.cpp
    src/gui/image_player_window.cpp
    src/gui/main_window.cpp
    src/main.cpp
    ../mkf_decompress.c
)

set(HEADERS
    include/core/utils/cache.h
    include/core/utils/resource_model.h
    include/core/utils/big5hkscs_table.h
    include/core/utils/flic.h
    include/core/utils/flic_details.h
    include/gui/graphics_text_window.h
    include/gui/map_graphics_view.h
    include/gui/map_panel_widget.h
    include/gui/image_player_widget.h
    include/gui/image_player_window.h
    include/gui/main_window.h
    ../mkf_decompress.h
)

add_executable(MkfViewer ${SOURCES} ${HEADERS})

target_include_directories(MkfViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MkfViewer Qt6::Core Qt6::Gui Qt6::Widgets ma_lib)

# --- MapPanelWidget API contract test (RED -> GREEN) ---
add_executable(test_map_panel_widget
    test_map_panel_widget.cpp
    src/gui/map_panel_widget.cpp
    include/gui/map_panel_widget.h
)
target_include_directories(test_map_panel_widget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(test_map_panel_widget Qt6::Core Qt6::Gui Qt6::Widgets ma_lib)