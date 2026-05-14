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
#include "oemaestro/OEMaestroTagConverter.h"
#include "oemaestro/MaestroWriter.h"
#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/OEWriteMaestro.h"

#include <oechem.h>
#include <oebio.h>
#include <oegrid.h>

using namespace OEMaestro;

// Force the linker to include oeio_handler.o from liboemaestro.a
// so that the OEIO_REGISTER_FORMAT static initializer runs.
#ifdef OEMAESTRO_HAS_OEIO
namespace OEMaestro { void oemaestro_force_link_oeio_handler(); }
static struct _OemaestroOeioForceLink {
    _OemaestroOeioForceLink() { OEMaestro::oemaestro_force_link_oeio_handler(); }
} _oemaestro_oeio_force_link;
#endif

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
// These enable typemaps for OpenEye types whose definitions live in the
// OpenEye SWIG runtime (v4). Only types you actually use in your wrapped API
// need full #include — forward declarations suffice for the typemaps.

namespace OEChem {
    class OEMolBase;
    class OEMCMolBase;
    class OEMol;
    class OEGraphMol;
    class OEAtomBase;
    class OEBondBase;
    class OEConfBase;
    class OEMatchBase;
    class OEMolDatabase;
    class oemolistream;
    class oemolostream;
    class OEQMol;
    class OEResidue;
    class OEUniMolecularRxn;
    class OEUnaryAtomPred;
}

namespace OEBio {
    class OEDesignUnit;
    class OEHierView;
    class OEHierResidue;
    class OEHierFragment;
    class OEHierChain;
    class OEInteractionHint;
    class OEInteractionHintContainer;
}

namespace OEDocking {
    class OEReceptor;
}

namespace OEPlatform {
    class oeifstream;
    class oeofstream;
    class oeisstream;
    class oeosstream;
}

namespace OESystem {
    class OEScalarGrid;
    class OERecord;
    class OEMolRecord;
}

// ============================================================================
// Cross-runtime SWIG compatibility layer
// ============================================================================
// OpenEye's Python bindings use SWIG runtime v4; our module uses v5.
// Since the runtimes are separate, SWIG_TypeQuery cannot access OpenEye types.
// We use Python isinstance for type safety and directly extract the void*
// pointer from the SwigPyObject struct layout (stable across SWIG versions).
//
// This approach enables passing OpenEye objects between Python and C++ without
// serialization. The macros below generate the boilerplate for each type.

%{
// Minimal SwigPyObject layout compatible across SWIG runtime versions.
// The actual struct may have more fields, but ptr is always first after
// PyObject_HEAD.
struct _SwigPyObjectCompat {
    PyObject_HEAD
    void *ptr;
};

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

// ---- Type checker generator macro ----
// Generates a cached isinstance checker for an OpenEye Python type.
// TAG:    identifier suffix (e.g., oemolbase)
// MODULE: Python module string (e.g., "openeye.oechem")
// CLASS:  Python class name string (e.g., "OEMolBase")
#define DEFINE_OE_TYPE_CHECKER(TAG, MODULE, CLASS) \
    static PyObject* _oemaestro_oe_##TAG##_type = NULL; \
    static bool _oemaestro_is_##TAG(PyObject* obj) { \
        if (!_oemaestro_oe_##TAG##_type) { \
            PyObject* mod = PyImport_ImportModule(MODULE); \
            if (mod) { \
                _oemaestro_oe_##TAG##_type = PyObject_GetAttrString(mod, CLASS); \
                Py_DECREF(mod); \
            } \
            if (!_oemaestro_oe_##TAG##_type) return false; \
        } \
        return PyObject_IsInstance(obj, _oemaestro_oe_##TAG##_type) == 1; \
    }

// ---- Molecule types (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oemolbase,    "openeye.oechem", "OEMolBase")
DEFINE_OE_TYPE_CHECKER(oemcmolbase,  "openeye.oechem", "OEMCMolBase")
DEFINE_OE_TYPE_CHECKER(oemol,        "openeye.oechem", "OEMol")
DEFINE_OE_TYPE_CHECKER(oegraphmol,   "openeye.oechem", "OEGraphMol")
DEFINE_OE_TYPE_CHECKER(oeqmol,       "openeye.oechem", "OEQMol")

// ---- Atom / bond / conformer / residue (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oeatombase,   "openeye.oechem", "OEAtomBase")
DEFINE_OE_TYPE_CHECKER(oebondbase,   "openeye.oechem", "OEBondBase")
DEFINE_OE_TYPE_CHECKER(oeconfbase,   "openeye.oechem", "OEConfBase")
DEFINE_OE_TYPE_CHECKER(oeresidue,    "openeye.oechem", "OEResidue")
DEFINE_OE_TYPE_CHECKER(oematchbase,  "openeye.oechem", "OEMatchBase")

// ---- Molecule I/O (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oemolistream, "openeye.oechem", "oemolistream")
DEFINE_OE_TYPE_CHECKER(oemolostream, "openeye.oechem", "oemolostream")
DEFINE_OE_TYPE_CHECKER(oemoldatabase,"openeye.oechem", "OEMolDatabase")

// ---- Reactions (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oeunimolecularrxn, "openeye.oechem", "OEUniMolecularRxn")

// ---- Predicates (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oeunaryatompred, "openeye.oechem", "OEUnaryAtomPred")

// ---- Platform streams (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oeifstream,   "openeye.oechem", "oeifstream")
DEFINE_OE_TYPE_CHECKER(oeofstream,   "openeye.oechem", "oeofstream")
DEFINE_OE_TYPE_CHECKER(oeisstream,   "openeye.oechem", "oeisstream")
DEFINE_OE_TYPE_CHECKER(oeosstream,   "openeye.oechem", "oeosstream")

// ---- Records (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oerecord,     "openeye.oechem", "OERecord")
DEFINE_OE_TYPE_CHECKER(oemolrecord,  "openeye.oechem", "OEMolRecord")

// ---- Bio / hierarchy (openeye.oechem) ----
DEFINE_OE_TYPE_CHECKER(oedesignunit, "openeye.oechem", "OEDesignUnit")
DEFINE_OE_TYPE_CHECKER(oehierview,   "openeye.oechem", "OEHierView")
DEFINE_OE_TYPE_CHECKER(oehierresidue,"openeye.oechem", "OEHierResidue")
DEFINE_OE_TYPE_CHECKER(oehierfragment,"openeye.oechem","OEHierFragment")
DEFINE_OE_TYPE_CHECKER(oehierchain,  "openeye.oechem", "OEHierChain")
DEFINE_OE_TYPE_CHECKER(oeinteractionhint,          "openeye.oechem", "OEInteractionHint")
DEFINE_OE_TYPE_CHECKER(oeinteractionhintcontainer, "openeye.oechem", "OEInteractionHintContainer")

// ---- Grid (openeye.oegrid) ----
DEFINE_OE_TYPE_CHECKER(oescalargrid, "openeye.oegrid", "OEScalarGrid")

// ---- Docking (openeye.oedocking) ----
DEFINE_OE_TYPE_CHECKER(oereceptor,   "openeye.oedocking", "OEReceptor")

#undef DEFINE_OE_TYPE_CHECKER

// ---- OEScalarGrid return-type helper (zero-copy pointer swap) ----
static PyObject* _oemaestro_wrap_as_oe_grid(OESystem::OEScalarGrid* grid) {
    if (!grid) {
        Py_RETURN_NONE;
    }
    PyObject* oegrid_mod = PyImport_ImportModule("openeye.oegrid");
    if (!oegrid_mod) {
        delete grid;
        return NULL;
    }
    PyObject* grid_cls = PyObject_GetAttrString(oegrid_mod, "OEScalarGrid");
    Py_DECREF(oegrid_mod);
    if (!grid_cls) {
        delete grid;
        return NULL;
    }
    PyObject* oe_grid = PyObject_CallNoArgs(grid_cls);
    Py_DECREF(grid_cls);
    if (!oe_grid) {
        delete grid;
        return NULL;
    }
    PyObject* thisAttr = PyObject_GetAttrString(oe_grid, "this");
    if (!thisAttr) {
        PyErr_Clear();
        Py_DECREF(oe_grid);
        delete grid;
        return NULL;
    }
    _SwigPyObjectCompat* swig_this = (_SwigPyObjectCompat*)thisAttr;
    delete reinterpret_cast<OESystem::OEScalarGrid*>(swig_this->ptr);
    swig_this->ptr = grid;
    Py_DECREF(thisAttr);
    return oe_grid;
}
%}

// ============================================================================
// Typemap generator macros
// ============================================================================

// Generate const-ref and non-const-ref typemaps for a cross-runtime OpenEye type.
// CPP_TYPE: fully qualified C++ type (e.g., OEChem::OEMolBase)
// CHECKER:  isinstance checker function name
// ERR_MSG:  error message on type mismatch
%define OE_CROSS_RUNTIME_REF_TYPEMAPS(CPP_TYPE, CHECKER, ERR_MSG)

%typemap(in) const CPP_TYPE& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (CHECKER($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), ERR_MSG);
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) const CPP_TYPE& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : CHECKER($input) ? 1 : 0;
}

%typemap(in) CPP_TYPE& (void *argp = 0, int res = 0) {
    res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
    if (!SWIG_IsOK(res)) {
        if (CHECKER($input)) {
            argp = _oemaestro_extract_swig_ptr($input);
            if (argp) res = SWIG_OK;
        }
    }
    if (!SWIG_IsOK(res)) {
        SWIG_exception_fail(SWIG_ArgError(res), ERR_MSG);
    }
    if (!argp) {
        SWIG_exception_fail(SWIG_NullReferenceError, "Null reference.");
    }
    $1 = reinterpret_cast< $1_ltype >(argp);
}

%typemap(typecheck, precedence=10) CPP_TYPE& {
    void *vptr = 0;
    int res = SWIG_ConvertPtr($input, &vptr, $descriptor, SWIG_POINTER_NO_NULL);
    $1 = SWIG_IsOK(res) ? 1 : CHECKER($input) ? 1 : 0;
}

%enddef

// Generate nullable-pointer typemaps (accepts None) for a cross-runtime type.
%define OE_CROSS_RUNTIME_NULLABLE_PTR_TYPEMAPS(CPP_TYPE, CHECKER, ERR_MSG)

%typemap(in) const CPP_TYPE* (void *argp = 0, int res = 0) {
    if ($input == Py_None) {
        $1 = NULL;
    } else {
        res = SWIG_ConvertPtr($input, &argp, $descriptor, 0);
        if (!SWIG_IsOK(res)) {
            if (CHECKER($input)) {
                argp = _oemaestro_extract_swig_ptr($input);
                if (argp) res = SWIG_OK;
            }
        }
        if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_ArgError(res), ERR_MSG);
        }
        $1 = reinterpret_cast< $1_ltype >(argp);
    }
}

%typemap(typecheck, precedence=10) const CPP_TYPE* {
    if ($input == Py_None) {
        $1 = 1;
    } else {
        void *vptr = 0;
        int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
        $1 = SWIG_IsOK(res) ? 1 : CHECKER($input) ? 1 : 0;
    }
}

%enddef

// ============================================================================
// Typemap declarations for all OpenEye types
// ============================================================================
// Each type gets const-ref and non-const-ref typemaps. Types that commonly
// appear as optional parameters also get nullable-pointer typemaps.
// These are inert until a wrapped function signature uses the type.

// ---- Molecule hierarchy (OEChem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEMolBase,    _oemaestro_is_oemolbase,    "Expected OEMolBase-derived object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEMCMolBase,  _oemaestro_is_oemcmolbase,  "Expected OEMCMolBase-derived object (OEMCMolBase or OEMol).")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEMol,        _oemaestro_is_oemol,        "Expected OEMol object.")
// Note: OEMol& typemap is generated by the macro above, but OpenEye's OEMolWrapper*
// cannot be reinterpret_cast to OEMol* across SWIG runtimes. We only expose
// Read(OEMolBase&) which uses the working OEMolBase& typemap.
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEGraphMol,   _oemaestro_is_oegraphmol,   "Expected OEGraphMol object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEQMol,       _oemaestro_is_oeqmol,       "Expected OEQMol object.")

// ---- Atom / bond / conformer / residue / match (OEChem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEAtomBase,   _oemaestro_is_oeatombase,   "Expected OEAtomBase-derived object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEBondBase,   _oemaestro_is_oebondbase,   "Expected OEBondBase-derived object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEConfBase,   _oemaestro_is_oeconfbase,   "Expected OEConfBase-derived object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEResidue,    _oemaestro_is_oeresidue,    "Expected OEResidue object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEMatchBase,  _oemaestro_is_oematchbase,  "Expected OEMatchBase-derived object.")

// ---- Molecule I/O (OEChem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::oemolistream,  _oemaestro_is_oemolistream, "Expected oemolistream object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::oemolostream,  _oemaestro_is_oemolostream, "Expected oemolostream object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEMolDatabase, _oemaestro_is_oemoldatabase,"Expected OEMolDatabase object.")

// ---- Reactions (OEChem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEUniMolecularRxn, _oemaestro_is_oeunimolecularrxn, "Expected OEUniMolecularRxn object.")

// ---- Predicates (OEChem -- oemaestro-specific) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEChem::OEUnaryAtomPred, _oemaestro_is_oeunaryatompred, "Expected OEUnaryAtomPred object.")

// ---- Platform streams (OEPlatform) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEPlatform::oeifstream, _oemaestro_is_oeifstream, "Expected oeifstream object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEPlatform::oeofstream, _oemaestro_is_oeofstream, "Expected oeofstream object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEPlatform::oeisstream, _oemaestro_is_oeisstream, "Expected oeisstream object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEPlatform::oeosstream, _oemaestro_is_oeosstream, "Expected oeosstream object.")

// ---- Records (OESystem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OESystem::OERecord,    _oemaestro_is_oerecord,    "Expected OERecord object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OESystem::OEMolRecord, _oemaestro_is_oemolrecord, "Expected OEMolRecord object.")

// ---- Bio / hierarchy (OEBio) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEDesignUnit,   _oemaestro_is_oedesignunit, "Expected OEDesignUnit object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEHierView,     _oemaestro_is_oehierview,   "Expected OEHierView object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEHierResidue,  _oemaestro_is_oehierresidue,"Expected OEHierResidue object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEHierFragment,  _oemaestro_is_oehierfragment,"Expected OEHierFragment object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEHierChain,    _oemaestro_is_oehierchain,  "Expected OEHierChain object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEInteractionHint,          _oemaestro_is_oeinteractionhint,          "Expected OEInteractionHint object.")
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEBio::OEInteractionHintContainer, _oemaestro_is_oeinteractionhintcontainer, "Expected OEInteractionHintContainer object.")

// ---- Grid (OESystem) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OESystem::OEScalarGrid, _oemaestro_is_oescalargrid, "Expected OEScalarGrid-derived object.")
OE_CROSS_RUNTIME_NULLABLE_PTR_TYPEMAPS(OESystem::OEScalarGrid, _oemaestro_is_oescalargrid, "Expected OEScalarGrid or None.")

// OEScalarGrid return-type typemap (wraps C++ grid as native openeye.oegrid object)
%typemap(out) OESystem::OEScalarGrid* {
    $result = _oemaestro_wrap_as_oe_grid($1);
    if (!$result) SWIG_fail;
}

// ---- Docking (OEDocking) ----
OE_CROSS_RUNTIME_REF_TYPEMAPS(OEDocking::OEReceptor, _oemaestro_is_oereceptor, "Expected OEReceptor object.")

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
// Version macros
// ============================================================================
#define OEMAESTRO_VERSION_MAJOR 0
#define OEMAESTRO_VERSION_MINOR 7
#define OEMAESTRO_VERSION_PATCH 4

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

class OEMaestroReaderConfig {
public:
    OEMaestroReaderConfig();
    OEMaestroReaderConfig(OEMaestroTag tags, OEMaestroPerception perception,
                          unsigned int num_threads = 1);

    void SetTags(OEMaestroTag tags);
    OEMaestroTag GetTags() const;

    void SetPerception(OEMaestroPerception perception);
    OEMaestroPerception GetPerception() const;

    void SetNumThreads(unsigned int num_threads);
    unsigned int GetNumThreads() const;
};

enum OEMaestroWriteMode : unsigned int {
    WRITE_CREATE = 0,
    WRITE_APPEND = 1
};

class OEMaestroWriterConfig {
public:
    OEMaestroWriterConfig();
    OEMaestroWriterConfig(OEMaestroTag tags, OEMaestroWriteMode mode,
                          const std::string& default_owner = "user");
    void SetTags(OEMaestroTag tags);
    OEMaestroTag GetTags() const;
    void SetMode(OEMaestroWriteMode mode);
    OEMaestroWriteMode GetMode() const;
    void SetDefaultOwner(const std::string& owner);
    const std::string& GetDefaultOwner() const;
};

class OEMaestroTagConverter {
public:
    explicit OEMaestroTagConverter(OEMaestroTag tags = TAG_ALL,
                                   const std::string& default_owner = "user");
    std::string ToFormatted(const std::string& maestro_key) const;
    std::string ToMaestroTag(const std::string& formatted_key,
                             char type_hint = '\0') const;
    static bool IsFullMaestroKey(const std::string& key);
    OEMaestroTag GetTags() const;
    void SetTags(OEMaestroTag tags);
    const std::string& GetDefaultOwner() const;
    void SetDefaultOwner(const std::string& owner);
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

    void Convert(OEChem::OEMolBase& dst, const MaestroMol& src) const;
    void Convert(MaestroMol& dst, const OEChem::OEMolBase& src) const;

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

%threadallow OEMaestroReader::Read;

class OEMaestroReader {
public:
    explicit OEMaestroReader(const std::string& filename,
                             OEMaestroReaderConfig config = OEMaestroReaderConfig());
    // Note: oeifstream constructor not exposed to Python (handled in __init__.py)

    bool Read(OEChem::OEMol& mol);
    bool Read(OEChem::OEMolBase& mol);

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

// ============================================================================
// MaestroWriter (Layer 1) -- low-level IR writer
// ============================================================================

%ignore MaestroWriter(const MaestroWriter&);
%ignore MaestroWriter::operator=(const MaestroWriter&);
%ignore MaestroWriter::operator=(MaestroWriter&&);

%threadallow MaestroWriter::Write;
%threadallow MaestroWriter::Close;

class MaestroWriter {
public:
    explicit MaestroWriter(const std::string& filename,
                           OEMaestroWriteMode mode = WRITE_CREATE);
    // Note: shared_ptr<ostream> constructor not exposed to Python

    bool Write(const MaestroMol& mol);
    void Close();

    ~MaestroWriter();
    MaestroWriter(MaestroWriter&&) noexcept;
};

// ============================================================================
// OEMaestroWriter (Layer 3) -- main writer API
// ============================================================================

%ignore OEMaestroWriter(const OEMaestroWriter&);
%ignore OEMaestroWriter::operator=(const OEMaestroWriter&);
%ignore OEMaestroWriter::operator=(OEMaestroWriter&&);

%threadallow OEMaestroWriter::Write;
%threadallow OEMaestroWriter::Close;

class OEMaestroWriter {
public:
    explicit OEMaestroWriter(const std::string& filename,
                             OEMaestroWriteMode mode = WRITE_CREATE);
    OEMaestroWriter(const std::string& filename,
                    const OEMaestroWriterConfig& config);
    // Note: oeofstream constructors not exposed to Python (handled in __init__.py)

    bool Write(const OEChem::OEMolBase& mol);
    void Close();

    ~OEMaestroWriter();
    OEMaestroWriter(OEMaestroWriter&&) noexcept;
};

// ============================================================================
// Single-molecule OEWriteMaestro (SWIG-exposed overloads)
// ============================================================================
bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol);
bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol,
                    const OEMaestroWriterConfig& config);
// Note: oeofstream overloads NOT exposed (handled in __init__.py)

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
    return f"OEMaestroReader(tags={config.GetTags()}, perception={config.GetPerception()})"
%}
}

%extend OEMaestro::OEMaestroDesignUnitReader {
%pythoncode %{
def __repr__(self):
    config = self.GetConfig()
    return f"OEMaestroDesignUnitReader(tags={config.GetTags()}, perception={config.GetPerception()})"
%}
}

%extend OEMaestro::OEMaestroWriter {
%pythoncode %{
def __repr__(self):
    return "OEMaestroWriter()"
%}
}
