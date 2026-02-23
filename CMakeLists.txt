cmake_minimum_required(VERSION 3.21)
project(VibeFlow VERSION 1.0.0 LANGUAGES CXX C OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# whisper.cpp (with CoreML + Metal)
set(WHISPER_COREML ON CACHE BOOL "" FORCE)
set(WHISPER_METAL ON CACHE BOOL "" FORCE)
set(WHISPER_NO_METAL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/whisper.cpp)

# App sources — .mm files for Objective-C++
add_executable(VibeFlow MACOSX_BUNDLE
    src/main.cpp
    src/App.cpp
    src/Transcriber.cpp
    src/AudioCapture.cpp
    src/HotkeyMonitor.cpp
    src/TextPaster.mm
    src/ui/GlassBubble.mm
    src/ui/WaveformWidget.cpp
    src/ui/TrayIcon.cpp
)

target_include_directories(VibeFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/whisper.cpp/include
)

target_link_libraries(VibeFlow PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    whisper
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
    src/ui/GlassBubble.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc"
)
