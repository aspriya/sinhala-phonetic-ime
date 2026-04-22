add_library(sinhala_ime_engine STATIC
    src/engine.cpp
    src/mapping.cpp
    src/composition.cpp
    src/trie.cpp
)

target_include_directories(sinhala_ime_engine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sinhala_ime_engine
    PRIVATE nlohmann_json::nlohmann_json
)

target_compile_features(sinhala_ime_engine PUBLIC cxx_std_20)

if(SINHALA_IME_BUILD_TESTS)
    add_subdirectory(tests)
endif()
