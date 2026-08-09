cmake_minimum_required(VERSION 3.20)
project(webview-cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)

add_executable(webview-cpp
    main.example.cc
)

if(MSVC)
    # Flags for MSVC
    target_compile_options(webview-cpp PRIVATE /W4 /wd4100 /wd4101 /wd4996)
    
    # Prevent windows.h from defining min/max macros and reduce the size of the Win32 header files
    target_compile_definitions(webview-cpp PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN) 
else()
    # Flags for GCC / Clang
    target_compile_options(webview-cpp PRIVATE -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated-declarations)
endif()

target_include_directories(webview-cpp PRIVATE .)

target_link_libraries(webview-cpp PRIVATE
    comctl32
    version
    shlwapi
    dwmapi
)