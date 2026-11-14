cmake_minimum_required(VERSION 3.16)
project(noldoj-cli CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 跨平台构建说明（无需改动源码，按目标平台选择工具链）：
#   Windows x64     : cmake -S . -B build -G Ninja
#   Windows x86     : cmake -S . -B build -G Ninja -DCMAKE_CXX_FLAGS=-m32
#                     （MSVC 使用 -A Win32）
#   Windows arm64   : cmake -S . -B build -G "Visual Studio 17 2022" -A ARM64
#   Linux x86/x64   : cmake -S . -B build -G Ninja            （自动链接 libcurl）
#   Linux arm64     : cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
#   macOS x64/arm64 : cmake -S . -B build -G Ninja            （使用系统 libcurl）
#                     如使用 Homebrew 的 curl：-DCMAKE_PREFIX_PATH=$(brew --prefix curl)
#   Android/Termux  : pkg install cmake ninja libcurl pkg-config
#                     cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$PREFIX
#                     （arm64 原生编译）
#   Android NDK     : cmake -S . -B build -G Ninja \
#                       -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#                       -DANDROID_ABI=arm64-v8a
# Windows 平台使用系统自带 WinHTTP，无第三方依赖；其余平台使用 libcurl。

# 版本号以 package.json 为唯一来源（发布时用 `npm version patch|minor|major` 更新）。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/package.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.json" NOLDOJ_PACKAGE_JSON)
    string(REGEX MATCH "\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _version_match "${NOLDOJ_PACKAGE_JSON}")
    set(NOLDOJ_VERSION "${CMAKE_MATCH_1}")
endif()
if(NOT NOLDOJ_VERSION)
    set(NOLDOJ_VERSION "0.0.0")
endif()

if(WIN32)
    list(APPEND NOLDOJ_NET_SOURCE src/net/winhttp_transport.cpp)
else()
    find_package(CURL REQUIRED)
    list(APPEND NOLDOJ_NET_SOURCE src/net/curl_transport.cpp)
endif()

add_executable(noldoj
    src/main.cpp
    src/core/url.cpp
    src/core/config.cpp
    src/core/crypto.cpp
    src/core/keyring.cpp
    src/core/auth.cpp
    src/core/api_client.cpp
    ${NOLDOJ_NET_SOURCE}
    src/cli/commands.cpp
    src/cli/runner.cpp
)

target_include_directories(noldoj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(noldoj PRIVATE NOLDOJ_VERSION="${NOLDOJ_VERSION}")

if(WIN32)
    # advapi32: 凭据管理器（CredRead/CredWrite）；bcrypt: 密码学随机数
    target_link_libraries(noldoj PRIVATE winhttp shell32 advapi32 bcrypt)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(noldoj PRIVATE CURL::libcurl Threads::Threads)
endif()

if(MSVC)
    target_compile_options(noldoj PRIVATE /W4 /utf-8)
else()
    target_compile_options(noldoj PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
