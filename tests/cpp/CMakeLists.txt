include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(oemaestro_tests
    test_main.cpp
    test_maestro_mol.cpp
    test_maestro_reader.cpp
    test_mol_converter.cpp
    test_oemaestro_reader.cpp
    test_residue_classifier.cpp
    test_bounded_queue.cpp
    test_design_unit_reader.cpp
    test_threaded_reader.cpp
    test_maestro_tag_converter.cpp
    test_maestro_writer.cpp
    test_oemaestro_writer.cpp
    test_oe_write_maestro.cpp
)

target_link_libraries(oemaestro_tests
    PRIVATE
        oemaestro
        GTest::gtest
        maeparser
)

target_compile_definitions(oemaestro_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

include(GoogleTest)
gtest_discover_tests(oemaestro_tests)
