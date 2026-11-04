cmake_minimum_required(VERSION 3.25)
project(OpenRoute VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Definitions
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler warnings and options
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /utf-8 /EHsc)
    # Silence warnings in third party C code
    set(THIRD_PARTY_C_FLAGS "/W0")
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    set(THIRD_PARTY_C_FLAGS "-w")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# ----------------- Third Party Libraries -----------------
# 1. Monocypher
add_library(monocypher STATIC
    third_party/monocypher/monocypher.c
    third_party/monocypher/monocypher.h
)
target_include_directories(monocypher PUBLIC third_party/monocypher)
if(MSVC)
    target_compile_options(monocypher PRIVATE /W0)
else()
    target_compile_options(monocypher PRIVATE -w)
endif()

# 2. SQLite3
add_library(sqlite3 STATIC
    third_party/sqlite3/sqlite3.c
    third_party/sqlite3/sqlite3.h
)
target_include_directories(sqlite3 PUBLIC third_party/sqlite3)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ----------------- Core Engine Library -----------------
file(GLOB_RECURSE CORE_SOURCES
    core/common/*.cpp
    core/security/*.cpp
    core/storage/*.cpp
    core/tunnel/*.cpp
    core/gateway/*.cpp
    core/device/*.cpp
    core/policy/*.cpp
    core/bandwidth/*.cpp
    core/accounting/*.cpp
    core/diagnostics/*.cpp
    core/networking/*.cpp
    core/relay/*.cpp
    core/ui/*.cpp
)

file(GLOB_RECURSE CORE_HEADERS
    core/common/*.hpp
    core/security/*.hpp
    core/storage/*.hpp
    core/tunnel/*.hpp
    core/gateway/*.hpp
    core/device/*.hpp
    core/policy/*.hpp
    core/bandwidth/*.hpp
    core/accounting/*.hpp
    core/diagnostics/*.hpp
    core/networking/*.hpp
    core/relay/*.hpp
    core/ui/*.hpp
)

add_library(openroute_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_link_libraries(openroute_core PUBLIC
    monocypher
    sqlite3
)

if(WIN32)
    target_link_libraries(openroute_core PUBLIC
        ws2_32
        iphlpapi
        crypt32
        bcrypt
        comctl32
        gdi32
        user32
        shell32
        ole32
    )
endif()

# ----------------- Main Gateway Executable -----------------
add_executable(openroute_gateway
    apps/windows/main.cpp
    apps/windows/win_gui.cpp
)

target_link_libraries(openroute_gateway PRIVATE
    openroute_core
)

# ----------------- Automated Tests -----------------
enable_testing()

add_executable(openroute_unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_crypto.cpp
    tests/unit/test_database.cpp
    tests/integration/test_tunnel_handshake.cpp
    tests/integration/test_gateway_pipeline.cpp
    tests/integration/test_client_engine.cpp
    tests/unit/test_nat_and_relay.cpp
    tests/integration/test_http_server.cpp
)

target_link_libraries(openroute_unit_tests PRIVATE
    openroute_core
)

add_test(NAME UnitTests COMMAND openroute_unit_tests)
