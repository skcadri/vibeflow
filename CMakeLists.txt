cmake_minimum_required(VERSION 3.21)
project(VibeFlow VERSION 1.0.0 LANGUAGES CXX C OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# transcribe.cpp — ggml speech-to-text for Qwen3-ASR + Whisper (Metal on Apple Silicon)
set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
set(TRANSCRIBE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TRANSCRIBE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/transcribe.cpp)
# transcribe.cpp resolves its public include dir via CMAKE_SOURCE_DIR, which is
# *our* root when it's a subdirectory — point it at its real include dir.
target_include_directories(transcribe PUBLIC ${CMAKE_SOURCE_DIR}/deps/transcribe.cpp/include)

# qt-liquid-glass
add_subdirectory(deps/qt-liquid-glass)

# App sources — .mm files for Objective-C++
add_executable(VibeFlow MACOSX_BUNDLE
    src/main.cpp
    src/App.cpp
    src/Transcriber.cpp
    src/TranscriptionServer.cpp
    src/AudioCapture.cpp
    src/HotkeyMonitor.mm
    src/TextPaster.mm
    src/data/SettingsManager.cpp
    src/ui/GlassBubble.mm
    src/ui/WaveformWidget.cpp
    src/ui/TrayIcon.cpp
    src/ui/RecentTranscriptionsDialog.cpp
    src/ui/VocabularyDialog.cpp
)

target_include_directories(VibeFlow PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/httplib
)

target_link_libraries(VibeFlow PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    transcribe
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
