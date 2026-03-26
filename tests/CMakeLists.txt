include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(oemaestro_tests
    cpp/test_main.cpp
    cpp/test_maestro_mol.cpp
    cpp/test_maestro_reader.cpp
    cpp/test_mol_converter.cpp
    cpp/test_oemaestro_reader.cpp
    cpp/test_residue_classifier.cpp
    cpp/test_bounded_queue.cpp
    cpp/test_design_unit_reader.cpp
    cpp/test_threaded_reader.cpp
)

target_link_libraries(oemaestro_tests
    PRIVATE
        oemaestro
        GTest::gtest
        maeparser
)

target_compile_definitions(oemaestro_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

include(GoogleTest)
gtest_discover_tests(oemaestro_tests)
