cmake_minimum_required(VERSION 3.24)
project(AudioScopeByBerryStudio VERSION 0.7.0 LANGUAGES C CXX)

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

if(MSVC)
    target_compile_options(audioscope_core PRIVATE /W4)
    target_compile_options(audioscope_ui PRIVATE /W4)
    target_compile_options(audioscope_app PRIVATE /W4)
else()
    target_compile_options(audioscope_ui PRIVATE -Wall -Wextra)
    target_compile_options(audioscope_app PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME alpha_selftest COMMAND audioscope-alpha selftest)
add_test(NAME alpha_scope COMMAND audioscope-alpha scope)
add_test(NAME alpha_fft COMMAND audioscope-alpha fft)
add_test(NAME alpha_ui COMMAND audioscope-alpha ui
         --web-src "${CMAKE_CURRENT_SOURCE_DIR}/web"
         --out "${CMAKE_CURRENT_BINARY_DIR}/ui-test")
