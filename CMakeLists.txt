cmake_minimum_required(VERSION 3.24)

project(SysGlance VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SysGlance WIN32
    src/main.cpp
    src/config.cpp
    src/metrics.cpp
    src/ui.cpp
    src/sysglance.rc
)

target_include_directories(SysGlance PRIVATE src)

target_compile_definitions(SysGlance PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

if (MSVC)
    target_compile_options(SysGlance PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_compile_definitions(SysGlance PRIVATE _CRT_SECURE_NO_WARNINGS)
    # GitHub Release assets should run without a separately installed VC++ runtime.
    set_property(TARGET SysGlance PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(SysGlance PRIVATE
    advapi32
    comdlg32
    d2d1
    dwrite
    dxgi
    iphlpapi
    pdh
    shell32
    shcore
    user32
)

if (BUILD_TESTING)
    add_executable(SysGlanceMathTests tests/math_tests.cpp)
    target_include_directories(SysGlanceMathTests PRIVATE src)
    target_compile_definitions(SysGlanceMathTests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    add_test(NAME SysGlanceMathTests COMMAND SysGlanceMathTests)
endif()

set_target_properties(SysGlance PROPERTIES
    OUTPUT_NAME "SysGlance"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
