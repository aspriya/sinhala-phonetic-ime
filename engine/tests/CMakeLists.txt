include(GoogleTest)

add_executable(sinhala_ime_tests
    test_trie.cpp
    test_mapping.cpp
    test_composition.cpp
    test_engine.cpp
)

target_link_libraries(sinhala_ime_tests
    PRIVATE
        sinhala_ime_engine
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

# Provide the path to the mapping file and golden-file corpus at test time.
target_compile_definitions(sinhala_ime_tests PRIVATE
    SINHALA_IME_MAPPING_PATH="${CMAKE_SOURCE_DIR}/mapping/phonetic-default.json"
    SINHALA_IME_GOLDEN_PATH="${CMAKE_SOURCE_DIR}/engine/tests/golden.txt"
)

gtest_discover_tests(sinhala_ime_tests)
