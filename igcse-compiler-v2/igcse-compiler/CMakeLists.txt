cmake_minimum_required(VERSION 3.14)
project(igcse-compiler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Lexer.cpp
    src/AST.cpp
    src/Parser.cpp
    src/CodeGen.cpp
)

include_directories(include)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(igcse-compiler ${SOURCES})

target_compile_options(igcse-compiler PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS igcse-compiler DESTINATION bin)

# ── Windows cross-compile target ──────────────────────────────────────────────
# Use: cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/win64-toolchain.cmake
