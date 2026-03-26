// swig/oemaestro.i
// SWIG interface file for OEMaestro Python bindings
%module _oemaestro

%{
#include "oemaestro/oemaestro.h"
#include "oemaestro/Error.h"
#include "oemaestro/Enums.h"
#include "oemaestro/MaestroMol.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/OEReadMaestro.h"
#include "oemaestro/ResidueClassifier.h"
#include "oemaestro/OEMaestroDesignUnitReader.h"
#include "oemaestro/OEReadMaestroDesignUnit.h"

#include <oechem.h>
#include <oebio.h>

using namespace OEMaestro;

// Typedef so SWIG-generated code can resolve OEChem::OEUnaryAtomPred.
// OpenEye headers define this as OESystem::OEUnaryPredicate<OEChem::OEAtomBase>
// but do not provide a short alias in the OEChem namespace.
namespace OEChem {
    typedef OESystem::OEUnaryPredicate<OEChem::OEAtomBase> OEUnaryAtomPred;
}
%}

// ============================================================================
// Forward declarations for cross-module SWIG type resolution
// ============================================================================
namespace OEChem {
    class OEMolBase;
    class OEMol;
    class OEUnaryAtomPred;
}
namespace OEBio {
    class OEDesignUnit;
}

// ============================================================================
// Cross-runtime pointer extraction (same pattern as oeselect)
// ============================================================================
// OpenEye uses SWIG runtime v4; our module uses v5. Since the runtimes are
// separate, SWIG_TypeQuery and SWIG_Python_GetSwigThis cannot access OpenEye
// types. We use Python isinstance for type safety and directly extract the
// void* pointer from the SwigPyObject struct layout (stable across versions).

%{
// Minimal SwigPyObject layout compatible across SWIG runtime versions.
// The actual struct may have more fields, but ptr is always first after
// PyObject_HEAD.
struct _SwigPyObjectCompat {
    PyObject_HEAD
    void *ptr;
};

static PyObject* _oemaestro_oe_molbase_type = NULL;

static bool _oemaestro_is_oemolbase(PyObject* obj) {
    if (!_oemaestro_oe_molbase_type) {
        PyObject* mod = PyImport_ImportModule("openeye.oechem");
        if (mod) {
            _oemaestro_oe_molbase_type = PyObject_GetAttrString(mod, "OEMolBase");
            Py_DECREF(mod);
        }
        if (!_oemaestro_oe_molbase_type) return false;
    }
    return PyObject_IsInstance(obj, _oemaestro_oe_molbase_type) == 1;
}

static void* _oemaestro_extract_swig_ptr(PyObject* obj) {
    PyObject* thisAttr = PyObject_GetAttrString(obj, "this");
    if (!thisAttr) {
        PyErr_Clear();
        return NULL;
    }
    void* ptr = ((_SwigPyObjectCompat*)thisAttr)->ptr;
    Py_DECREF(thisAttr);
    return ptr;
}

static PyObject* _oemaestro_oe_designunit_type = NULL;

static bool _oemaestro_is_oedesignunit(PyObject* obj) {
    if (!_oemaestro_oe_designunit_type) {
        PyObject* mod = PyImport_ImportModule("openeye.oechem");
        if (mod) {
            _oemaestro_oe_designunit_type = PyObject_GetAttrString(mod, "OEDesignUnit");
            Py_DECREF(mod);
        }
        if (!_oemaestro_oe_designunit_type) return false;
    }
    return PyObject_IsInstance(obj, _oemaestro_oe_designunit_type) == 1;
}

static PyObject* _oemaestro_oe_unaryatompred_type = NULL;

static bool _oemaestro_is_oeunaryatompred(PyObject* obj) {
    if (!_oemaestro_oe_unaryatompred_type) {
        PyObject* mod = PyImport_ImportModule("openeye.oechem");
        if (mod) {
            _oemaestro_oe_unaryatompred_type = PyObject_GetAttrString(mod, "OEUnaryAtomPred");
            Py_DECREF(mod);
        }
        if (!_oemaestro_oe_unaryatompred_type) return false;
    }
    return PyObject_IsInstance(obj, _oemaestro_oe_unaryatompred_type) == 1;
}

%}

// ============================================================================
// Typemap for OEChem::OEMolBase& (non-const)
// ============================================================================
%typemap(in) OEChem::OEMolBase& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (_oemaestro_is_oemolbase($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), "Expected OEMolBase-derived object.");
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null OEMolBase reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) OEChem::OEMolBase& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : _oemaestro_is_oemolbase($input) ? 1 : 0;
}

// ============================================================================
// Typemap for const OEChem::OEMolBase&
// ============================================================================
%typemap(in) const OEChem::OEMolBase& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (_oemaestro_is_oemolbase($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), "Expected OEMolBase-derived object.");
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null OEMolBase reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) const OEChem::OEMolBase& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : _oemaestro_is_oemolbase($input) ? 1 : 0;
}

// Note: No OEMol& typemap. OpenEye's OEMolWrapper* cannot be reinterpret_cast
// to OEMol* across SWIG runtimes. We only expose Read(OEMolBase&) which uses
// the working OEMolBase& typemap above.

// ============================================================================
// Typemap for OEBio::OEDesignUnit& (non-const)
// ============================================================================
%typemap(in) OEBio::OEDesignUnit& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (_oemaestro_is_oedesignunit($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), "Expected OEDesignUnit object.");
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null OEDesignUnit reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) OEBio::OEDesignUnit& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : _oemaestro_is_oedesignunit($input) ? 1 : 0;
}

// ============================================================================
// Typemap for const OEChem::OEUnaryAtomPred& (predicate objects)
// ============================================================================
%typemap(in) const OEChem::OEUnaryAtomPred& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (_oemaestro_is_oeunaryatompred($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), "Expected OEUnaryAtomPred object.");
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null OEUnaryAtomPred reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) const OEChem::OEUnaryAtomPred& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : _oemaestro_is_oeunaryatompred($input) ? 1 : 0;
}

// ============================================================================
// STL typemaps
// ============================================================================
%include "std_string.i"
%include "std_vector.i"
%include "std_map.i"
%include "stdint.i"
%include "exception.i"

// ============================================================================
// Exception handling
// ============================================================================
%exception {
    try {
        $action
    } catch (const OEMaestro::MaestroParseError& e) {
        SWIG_exception(SWIG_IOError, e.what());
    } catch (const OEMaestro::MaestroConvertError& e) {
        SWIG_exception(SWIG_ValueError, e.what());
    } catch (const OEMaestro::OEMaestroError& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (const std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (...) {
        SWIG_exception(SWIG_RuntimeError, "Unknown C++ exception");
    }
}

// ============================================================================
// Template instantiations for container types
// ============================================================================
%template(StringStringMap) std::map<std::string, std::string>;
%template(MaestroAtomVector) std::vector<OEMaestro::MaestroAtom>;
%template(MaestroBondVector) std::vector<OEMaestro::MaestroBond>;

// ============================================================================
// Enums
// ============================================================================
namespace OEMaestro {

enum OEMaestroTag : unsigned int {
    TAG_NONE  = 0x0,
    TAG_TYPE  = 0x1,
    TAG_OWNER = 0x2,
    TAG_NAME  = 0x4,
    TAG_ALL   = 0x7
};

enum OEMaestroPerception : unsigned int {
    PERCEPTION_NONE               = 0x0,
    PERCEPTION_CONNECTIVITY       = 0x1,
    PERCEPTION_RINGS              = 0x2,
    PERCEPTION_BOND_ORDERS        = 0x4,
    PERCEPTION_IMPLICIT_HYDROGENS = 0x8,
    PERCEPTION_FORMAL_CHARGES     = 0x10,
    PERCEPTION_ALL                = 0x1f,
    PERCEPTION_DEFAULT            = 0x1a
};

struct OEMaestroReaderConfig {
    OEMaestroTag tags;
    OEMaestroPerception perception;
    OEMaestroReaderConfig();
};

// ============================================================================
// Error classes (for SWIG to see them)
// ============================================================================
class OEMaestroError : public std::runtime_error {
public:
    explicit OEMaestroError(const std::string& message);
};

class MaestroParseError : public OEMaestroError {
public:
    explicit MaestroParseError(const std::string& message);
};

class MaestroConvertError : public OEMaestroError {
public:
    explicit MaestroConvertError(const std::string& message);
};

// ============================================================================
// MaestroMol IR (low-level debug API)
// ============================================================================
struct MaestroAtom {
    int atomic_number;
    double x, y, z;
    int formal_charge;
    std::string atom_name;
    std::string residue_name;
    int residue_number;
    std::string chain_id;
    std::string insert_code;
    double bfactor;
    double occupancy;
    bool is_ligand_atom;
    std::map<std::string, std::string> properties;
};

struct MaestroBond {
    int atom1_index;
    int atom2_index;
    int order;
};

struct MaestroMol {
    std::string title;
    std::vector<MaestroAtom> atoms;
    std::vector<MaestroBond> bonds;
    std::map<std::string, std::string> ct_properties;

    size_t NumAtoms() const;
    size_t NumBonds() const;
    std::string ToString() const;
};

// ============================================================================
// MaestroReader (Layer 1)
// ============================================================================

// Ignore copy constructor and assignment before class declaration
%ignore MaestroReader(const MaestroReader&);
%ignore MaestroReader::operator=(const MaestroReader&);
%ignore MaestroReader::operator=(MaestroReader&&);

class MaestroReader {
public:
    explicit MaestroReader(const std::string& filename);
    // Note: shared_ptr<istream> constructor not exposed to Python

    bool Read(MaestroMol& mol);

    ~MaestroReader();
    MaestroReader(MaestroReader&&) noexcept;
};

// ============================================================================
// MolConverter (Layer 2)
// ============================================================================
class MolConverter {
public:
    MolConverter();
    explicit MolConverter(OEMaestroTag tags, OEMaestroPerception perception = PERCEPTION_ALL);

    void Convert(const MaestroMol& maestro_mol, OEChem::OEMolBase& mol) const;

    void SetTagFormat(OEMaestroTag tags);
    OEMaestroTag GetTagFormat() const;
    void SetPerception(OEMaestroPerception perception);
    OEMaestroPerception GetPerception() const;
};

// ============================================================================
// OEMaestroReader (Layer 3) -- main public API
// ============================================================================

// Ignore SetConfTest (handled in Python), copy ops, move assign before class.
// Also ignore Read(OEMol&) -- OpenEye's OEMolWrapper* cannot be reinterpret_cast
// to OEMol* across SWIG runtimes. Only expose Read(OEMolBase&).
%ignore OEMaestroReader::SetConfTest;
%ignore OEMaestroReader::Read(OEChem::OEMol&);
%ignore OEMaestroReader(const OEMaestroReader&);
%ignore OEMaestroReader::operator=(const OEMaestroReader&);
%ignore OEMaestroReader::operator=(OEMaestroReader&&);

class OEMaestroReader {
public:
    explicit OEMaestroReader(const std::string& filename,
                             OEMaestroReaderConfig config = OEMaestroReaderConfig());
    // Note: oeifstream constructor not exposed to Python (handled in __init__.py)

    bool Read(OEChem::OEMol& mol);
    bool Read(OEChem::OEMolBase& mol);

    void SetPerception(OEMaestroPerception perception);
    void SetTagFormat(OEMaestroTag tags);
    OEMaestroPerception GetPerception() const;
    OEMaestroTag GetTagFormat() const;
    OEMaestroReaderConfig GetConfig() const;

    ~OEMaestroReader();
    OEMaestroReader(OEMaestroReader&&) noexcept;
};

// ============================================================================
// Single-molecule OEReadMaestro (SWIG-exposed overloads)
// ============================================================================
bool OEReadMaestro(const std::string& filename, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config = OEMaestroReaderConfig());
// Note: iterator-returning overloads NOT exposed (move-only return)
// Note: oeifstream overloads NOT exposed (handled in __init__.py)

// ============================================================================
// OEMaestroDesignUnitReader
// ============================================================================

%ignore OEMaestroDesignUnitReader(const OEMaestroDesignUnitReader&);
%ignore OEMaestroDesignUnitReader::operator=(const OEMaestroDesignUnitReader&);
%ignore OEMaestroDesignUnitReader::operator=(OEMaestroDesignUnitReader&&);

class OEMaestroDesignUnitReader {
public:
    explicit OEMaestroDesignUnitReader(const std::string& filename,
                                       OEMaestroReaderConfig config = OEMaestroReaderConfig());
    // Note: oeifstream constructor not exposed to Python (handled in __init__.py)

    bool Read(OEBio::OEDesignUnit& du);

    void SetLigandPredicate(const OEChem::OEUnaryAtomPred& pred);
    void SetSolventPredicate(const OEChem::OEUnaryAtomPred& pred);
    void SetCofactorPredicate(const OEChem::OEUnaryAtomPred& pred);

    void SetPerception(OEMaestroPerception perception);
    void SetTagFormat(OEMaestroTag tags);
    OEMaestroPerception GetPerception() const;
    OEMaestroTag GetTagFormat() const;
    OEMaestroReaderConfig GetConfig() const;

    ~OEMaestroDesignUnitReader();
    OEMaestroDesignUnitReader(OEMaestroDesignUnitReader&&) noexcept;
};

// ============================================================================
// Single-DU OEReadMaestroDesignUnit (SWIG-exposed overloads)
// ============================================================================
bool OEReadMaestroDesignUnit(const std::string& filename,
                              OEBio::OEDesignUnit& du,
                              OEMaestroReaderConfig config = OEMaestroReaderConfig());
// Note: iterator-returning overloads NOT exposed (move-only return)
// Note: oeifstream overloads NOT exposed (handled in __init__.py)

} // namespace OEMaestro

// ============================================================================
// Python extensions
// ============================================================================
%extend OEMaestro::MaestroMol {
%pythoncode %{
def __repr__(self):
    return self.ToString()
%}
}

%extend OEMaestro::MaestroAtom {
%pythoncode %{
def __repr__(self):
    return f"MaestroAtom(Z={self.atomic_number}, name='{self.atom_name}')"
%}
}

%extend OEMaestro::OEMaestroReader {
%pythoncode %{
def __repr__(self):
    config = self.GetConfig()
    return f"OEMaestroReader(tags={config.tags}, perception={config.perception})"
%}
}

%extend OEMaestro::OEMaestroDesignUnitReader {
%pythoncode %{
def __repr__(self):
    config = self.GetConfig()
    return f"OEMaestroDesignUnitReader(tags={config.tags}, perception={config.perception})"
%}
}
