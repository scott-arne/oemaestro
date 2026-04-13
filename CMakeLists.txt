cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

project(oemaestro VERSION 0.5.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OEMAESTRO_BUILD_TESTS "Build tests" ON)
option(OEMAESTRO_BUILD_PYTHON "Build Python bindings" ON)
option(OEMAESTRO_UNIVERSAL2 "Build universal2 binary for macOS" OFF)
option(OEMAESTRO_USE_STABLE_ABI "Use Python stable ABI" OFF)

if(APPLE)
    if(OEMAESTRO_UNIVERSAL2)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures" FORCE)
    else()
        # Clear any cached universal2 architectures from a previous run
        unset(CMAKE_OSX_ARCHITECTURES CACHE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    endif()
endif()

include(FetchContent)

FetchContent_Declare(
    cmake_openeye
    GIT_REPOSITORY https://github.com/scott-arne/cmake-openeye.git
    GIT_TAG v1.0.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cmake_openeye)
list(APPEND CMAKE_MODULE_PATH "${cmake_openeye_SOURCE_DIR}")

FetchContent_Declare(
    maeparser
    GIT_REPOSITORY https://github.com/schrodinger/maeparser.git
    GIT_TAG v1.3.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(maeparser)

if(OEMAESTRO_BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    find_package(SWIG 4.0)
endif()

if(OPENEYE_USE_SHARED)
    find_package(OpenEyePython REQUIRED)
endif()

find_package(OpenEye REQUIRED)

set(OEMAESTRO_SOURCES
    src/MaestroMol.cpp
    src/MaestroReader.cpp
    src/MolConverter.cpp
    src/StreamAdapter.cpp
    src/OEMaestroReader.cpp
    src/OEReadMaestro.cpp
    src/ResidueClassifier.cpp
    src/OEMaestroDesignUnitReader.cpp
    src/OEReadMaestroDesignUnit.cpp
    src/OEMaestroTagConverter.cpp
    src/MaestroWriter.cpp
    src/OEMaestroWriter.cpp
    src/OEWriteMaestro.cpp
)

add_library(oemaestro ${OEMAESTRO_SOURCES})
set_target_properties(oemaestro PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(oemaestro
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(oemaestro
    PUBLIC
        OpenEye::OEChem
    PRIVATE
        maeparser
)

# OEBio and OEGrid are optional targets -- in shared-library builds (e.g. from
# openeye-toolkits Python package) these symbols are already in liboechem, so a
# separate library may not exist.
if(TARGET OpenEye::OEBio)
    target_link_libraries(oemaestro PUBLIC OpenEye::OEBio)
endif()
if(TARGET OpenEye::OEGrid)
    target_link_libraries(oemaestro PUBLIC OpenEye::OEGrid)
endif()

if(OEMAESTRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OEMAESTRO_BUILD_PYTHON AND SWIG_FOUND)
    add_subdirectory(swig)
endif()

install(TARGETS oemaestro
    EXPORT oemaestro-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/oemaestro DESTINATION include)
install(EXPORT oemaestro-targets
    FILE OEMaestroTargets.cmake
    NAMESPACE OEMaestro::
    DESTINATION lib/cmake/oemaestro
)
