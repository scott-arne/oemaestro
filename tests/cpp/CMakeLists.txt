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
    test_oeio_handler.cpp
)

if(OEMAESTRO_BUILD_OEIO AND TARGET oeio)
    # Force whole-archive linking of oemaestro so that the static
    # OEIO_REGISTER_FORMAT initializer in oeio_handler.cpp is not stripped
    # by the linker (it has no externally referenced symbols).
    target_link_libraries(oemaestro_tests
        PRIVATE
            $<LINK_LIBRARY:WHOLE_ARCHIVE,oemaestro>
            GTest::gtest
            maeparser
            oeio
    )
else()
    target_link_libraries(oemaestro_tests
        PRIVATE
            oemaestro
            GTest::gtest
            maeparser
    )
endif()

target_compile_definitions(oemaestro_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

# Under a SHARED OpenEye build (dev/editable), the openeye-toolkits dylibs use
# @rpath install names, so the test executable needs an RPATH to the package's
# runtime library directory to load them (also needed by gtest_discover_tests).
if(OpenEye_LIBRARY_TYPE STREQUAL "SHARED" AND OPENEYE_RUNTIME_LIB_DIR)
    set_target_properties(oemaestro_tests PROPERTIES
        BUILD_RPATH "${OPENEYE_RUNTIME_LIB_DIR}")
endif()

include(GoogleTest)
gtest_discover_tests(oemaestro_tests)
