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
    INIT_PY ${CMAKE_SOURCE_DIR}/python/oemaestro/__init__.py
)
