include(OpenEyeSWIG)

openeye_add_swig_module(
    NAME oemaestro
    SWIG_FILE oemaestro.i
    LINK_LIBS oemaestro
    PYTHON_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/python/oemaestro
    STABLE_ABI ${OEMAESTRO_USE_STABLE_ABI}
    SWIG_FLAGS "-doxygen"
    EXPECTED_LIB_VARS
        OECHEM_LIBRARY OESYSTEM_LIBRARY OEPLATFORM_LIBRARY
        OEMATH_LIBRARY OEZSTD_LIBRARY
        OEGRID_LIBRARY OEFIZZCHEM_LIBRARY
    EXTRA_INSTALL_TARGETS maeparser
    INIT_PY ${CMAKE_SOURCE_DIR}/python/oemaestro/__init__.py
)

# Create SONAME symlink for maeparser in the Python output directory.
# EXTRA_INSTALL_TARGETS copies the versioned dylib (e.g., libmaeparser.1.3.2.dylib)
# but the SWIG module references the SONAME (libmaeparser.1.dylib).
get_target_property(_MAE_SOVERSION maeparser SOVERSION)
if(_MAE_SOVERSION)
    set(_PYOUT ${CMAKE_SOURCE_DIR}/python/oemaestro)
    add_custom_command(TARGET oemaestro_python POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE_NAME:maeparser>
            ${_PYOUT}/$<TARGET_SONAME_FILE_NAME:maeparser>
        COMMENT "Creating maeparser SONAME symlink in ${_PYOUT}"
    )
endif()
