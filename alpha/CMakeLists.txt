cmake_minimum_required(VERSION 3.24)
project(AudioScopeByBerryStudio VERSION 1.0.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(_as_default_platform macos)
elseif(WIN32)
    set(_as_default_platform windows)
else()
    set(_as_default_platform stub)
endif()

set(AUDIOSCOPE_PLATFORM "${_as_default_platform}" CACHE STRING
    "macos, windows, or stub")
set_property(CACHE AUDIOSCOPE_PLATFORM PROPERTY STRINGS macos windows stub)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS deployment target")
endif()

set(AS_CORE_SOURCES
    src/core/version.c
    src/core/units.c
    src/core/ring.c
    src/core/fft.c
    src/core/window.c
    src/core/stats.c
    src/core/generator.c
    src/core/input_mode.c
    src/core/pdf.c
    src/core/phase.c
    src/core/repair.c
    src/core/spectrogram.c
    src/core/tone.c
    src/core/safety.c
    src/core/calibration.c
    src/core/report.c
    src/core/session.c
)

add_library(audioscope_core STATIC ${AS_CORE_SOURCES})
target_include_directories(audioscope_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(NOT MSVC)
    target_compile_options(audioscope_core PRIVATE -Wall -Wextra)
endif()
if(UNIX)
    target_link_libraries(audioscope_core PUBLIC m)
endif()

add_library(audioscope_platform STATIC)
target_include_directories(audioscope_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(AUDIOSCOPE_PLATFORM STREQUAL "macos")
    if(NOT APPLE)
        message(FATAL_ERROR "AUDIOSCOPE_PLATFORM=macos requires an Apple host")
    endif()
    target_sources(audioscope_platform PRIVATE
        src/platform/macos/audio_coreaudio.c
        src/platform/macos/devices.c)
    target_link_libraries(audioscope_platform PUBLIC
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation")
elseif(AUDIOSCOPE_PLATFORM STREQUAL "windows")
    if(NOT WIN32)
        message(FATAL_ERROR "AUDIOSCOPE_PLATFORM=windows requires a Windows host")
    endif()
    target_sources(audioscope_platform PRIVATE
        src/platform/win/audio_winmm.c
        src/platform/win/devices_winmm.c)
    target_link_libraries(audioscope_platform PUBLIC winmm)
    target_compile_definitions(audioscope_platform PRIVATE _WIN32_WINNT=0x0601)
else()
    target_sources(audioscope_platform PRIVATE
        src/platform/stub/audio_null.c
        src/platform/stub/devices_stub.c)
endif()

add_library(audioscope_ui STATIC src/ui/ascii_canvas.cpp)
target_include_directories(audioscope_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(audioscope_ui PUBLIC audioscope_core)

add_library(audioscope_app STATIC src/app/alpha_app.cpp)
target_include_directories(audioscope_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(audioscope_app PUBLIC
    audioscope_core audioscope_platform audioscope_ui)

add_executable(audioscope-alpha src/app/main.cpp)
target_link_libraries(audioscope-alpha PRIVATE audioscope_app)

if(WIN32)
    add_executable(AudioScope WIN32
        src/app/native_model.cpp
        src/platform/win/native_app_win.cpp)
    target_include_directories(AudioScope PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(AudioScope PRIVATE
        audioscope_core audioscope_platform gdiplus shell32)
    target_compile_options(AudioScope PRIVATE /utf-8 /W4)
    target_compile_definitions(AudioScope PRIVATE _WIN32_WINNT=0x0601)
endif()

if(APPLE)
    add_executable(AudioScope MACOSX_BUNDLE
        src/app/native_model.cpp
        src/ui/native_pages.cpp
        src/ui/native_ui.c
        src/ui/native_theme.c
        src/platform/macos/native_app.mm
        src/platform/macos/native_main.mm
        assets/fonts/Geist-Regular.otf
        assets/fonts/Geist-Medium.otf
        assets/fonts/Geist-Bold.otf
        assets/fonts/GeistMono-Regular.otf)
    target_include_directories(AudioScope PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(AudioScope PRIVATE
        audioscope_core audioscope_platform
        "-framework Cocoa" "-framework CoreGraphics" "-framework CoreText"
        "-framework ImageIO" "-framework UniformTypeIdentifiers")
    set_target_properties(AudioScope PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "AudioScope"
        MACOSX_BUNDLE_GUI_IDENTIFIER "studio.berry.audioscope"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0")
    set_source_files_properties(
        src/platform/macos/native_app.mm src/platform/macos/native_main.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(
        assets/fonts/Geist-Regular.otf assets/fonts/Geist-Medium.otf
        assets/fonts/Geist-Bold.otf assets/fonts/GeistMono-Regular.otf
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
    if(NOT MSVC)
        target_compile_options(AudioScope PRIVATE -Wall -Wextra)
    endif()
endif()

if(MSVC)
    target_compile_options(audioscope_core PRIVATE /W4 /utf-8)
    target_compile_options(audioscope_ui PRIVATE /W4 /utf-8)
    target_compile_options(audioscope_app PRIVATE /W4 /utf-8)
else()
    target_compile_options(audioscope_ui PRIVATE -Wall -Wextra)
    target_compile_options(audioscope_app PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME alpha_selftest COMMAND audioscope-alpha selftest)
add_test(NAME alpha_scope COMMAND audioscope-alpha scope)
add_test(NAME alpha_fft COMMAND audioscope-alpha fft)
if(APPLE)
    add_test(NAME alpha_native_shot COMMAND AudioScope --shot
             "${CMAKE_CURRENT_BINARY_DIR}/native-shots")
    # 导航指示线动画的回归测试（时间驱动 / 从当前位置出发 / reduce-motion）。
    add_executable(alpha_nav_test
        tests/test_nav.c
        src/ui/native_ui.c
        src/ui/native_theme.c)
    target_include_directories(alpha_nav_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(alpha_nav_test PRIVATE
        "-framework CoreGraphics" "-framework CoreText"
        "-framework CoreFoundation")
    if(NOT MSVC)
        target_compile_options(alpha_nav_test PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME alpha_nav_indicator COMMAND alpha_nav_test)

    # 模型级可信度回归：CH2 独立 / 模拟计划 / FFT / 实际采样率。
    add_executable(alpha_model_test
        tests/test_model.cpp
        src/app/native_model.cpp)
    target_include_directories(alpha_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(alpha_model_test PRIVATE
        audioscope_core audioscope_platform
        "-framework CoreAudio" "-framework AudioToolbox"
        "-framework AudioUnit" "-framework CoreFoundation")
    if(NOT MSVC)
        target_compile_options(alpha_model_test PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME alpha_model_invariants COMMAND alpha_model_test)
endif()

