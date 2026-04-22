cmake_minimum_required(VERSION 3.20)
project(sinhala_phonetic_ime CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SINHALA_IME_BUILD_TESTS    "Build unit tests"     ON)
option(SINHALA_IME_ENABLE_COVERAGE "Enable gcov coverage (GCC/Clang)" OFF)

if(SINHALA_IME_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O0 -g --coverage)
        add_link_options(--coverage)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC)
    # /utf-8 is required so that UTF-8 source literals and \uNNNN escapes
    # survive compilation into the expected UTF-8 runtime bytes.
    add_compile_options(/W4 /permissive- /utf-8)
endif()

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

if(SINHALA_IME_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    # Prevent GoogleTest from overriding our compile options on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

add_subdirectory(engine)
