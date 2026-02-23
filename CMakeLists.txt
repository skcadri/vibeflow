cmake_minimum_required(VERSION 3.21)
project(VibeFlow VERSION 1.0.0 LANGUAGES CXX C OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# whisper.cpp (Metal GPU + ARM NEON + Accelerate BLAS on Apple Silicon)
set(WHISPER_COREML OFF CACHE BOOL "" FORCE)
set(WHISPER_METAL ON CACHE BOOL "" FORCE)
set(WHISPER_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/whisper.cpp)

# Fix: whisper.cpp's ggml-metal.m must compile as Objective-C (not plain C).
# Xcode 17 C compiler rejects implicit void* → id<MTLBuffer> conversions
# that are valid in ObjC. Force the language.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/deps/whisper.cpp/ggml-metal.m
    PROPERTIES LANGUAGE OBJC
)
# Suppress narrowing warnings in Metal backend (int64 → int32 for GPU dispatch params)
if(TARGET whisper)
    get_target_property(_whisper_sources whisper SOURCES)
    foreach(_src ${_whisper_sources})
        if(_src MATCHES "ggml-metal\\.m$")
            set_source_files_properties(${_src} PROPERTIES
                LANGUAGE OBJC
                COMPILE_FLAGS "-Wno-shorten-64-to-32"
            )
        endif()
    endforeach()
endif()

# qt-liquid-glass
add_subdirectory(deps/qt-liquid-glass)

# App sources — .mm files for Objective-C++
add_executable(VibeFlow MACOSX_BUNDLE
    src/main.cpp
    src/App.cpp
    src/Transcriber.cpp
    src/AudioCapture.cpp
    src/HotkeyMonitor.mm
    src/TextPaster.mm
    src/ui/GlassBubble.mm
    src/ui/WaveformWidget.cpp
    src/ui/TrayIcon.cpp
)

target_include_directories(VibeFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/whisper.cpp/include
    ${CMAKE_SOURCE_DIR}/deps/whisper.cpp
)

target_link_libraries(VibeFlow PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    whisper
    QtLiquidGlass
    "-framework CoreGraphics"
    "-framework AppKit"
    "-framework Carbon"
    "-framework ApplicationServices"
)

# macOS bundle settings
set_target_properties(VibeFlow PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/resources/Info.plist
    MACOSX_BUNDLE_ICON_FILE icon.icns
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.sohaib.vibeflow"
    MACOSX_BUNDLE_BUNDLE_NAME "VibeFlow"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
)

# Objective-C++ compile flags for .mm files
set_source_files_properties(
    src/TextPaster.mm
    src/HotkeyMonitor.mm
    src/ui/GlassBubble.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc"
)
