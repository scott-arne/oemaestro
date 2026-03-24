"""oemaestro -- Native Maestro format parser for OpenEye Toolkits.

Parses Schrodinger Maestro format files (.mae, .mae.gz, .maegz) into
native OpenEye molecules using the maeparser library.

Example usage::

    from openeye import oechem
    from oemaestro import OEMaestroReader, OEReadMaestro

    # Iterate over molecules in a file
    for mol in OEMaestroReader("input.mae"):
        print(oechem.OEMolToSmiles(mol))

    # Read a single molecule
    mol = oechem.OEGraphMol()
    OEReadMaestro("input.mae", mol)
"""
import os
import re
import warnings

__version__ = "0.1.0"
__version_info__ = (0, 1, 0)


def _ensure_library_compat():
    """Create compatibility symlinks when OpenEye library versions differ from build time.

    When oemaestro is built with shared OpenEye libraries, the compiled extension
    records the exact versioned library filenames (e.g., liboechem-4.3.0.1.dylib).
    If the user upgrades openeye-toolkits, these filenames change and the dynamic
    linker fails to load the extension.

    This function detects version mismatches and creates symlinks from the expected
    (build-time) library names to the actual (runtime) library files.
    """
    try:
        from . import _build_info
    except ImportError:
        return False

    if getattr(_build_info, 'OPENEYE_LIBRARY_TYPE', 'STATIC') != 'SHARED':
        return False

    expected_libs = getattr(_build_info, 'OPENEYE_EXPECTED_LIBS', [])
    if not expected_libs:
        return False

    try:
        from openeye import libs
        oe_lib_dir = libs.FindOpenEyeDLLSDirectory()
    except (ImportError, Exception):
        return False

    if not os.path.isdir(oe_lib_dir):
        return False

    pkg_dir = os.path.dirname(__file__)
    created_any = False

    for expected_name in expected_libs:
        if os.path.exists(os.path.join(oe_lib_dir, expected_name)):
            continue

        symlink_path = os.path.join(pkg_dir, expected_name)
        if os.path.islink(symlink_path):
            if os.path.exists(symlink_path):
                continue
            try:
                os.unlink(symlink_path)
            except OSError:
                continue
        elif os.path.exists(symlink_path):
            continue

        match = re.match(r'(lib\w+?)(-[\d.]+)?(\.[\d.]*\w+)$', expected_name)
        if not match:
            continue
        base_name = match.group(1)

        actual_path = None
        for f in os.listdir(oe_lib_dir):
            if f.startswith(base_name + '-') or f.startswith(base_name + '.'):
                actual_path = os.path.join(oe_lib_dir, f)
                break

        if actual_path:
            symlink_path = os.path.join(pkg_dir, expected_name)
            try:
                os.symlink(actual_path, symlink_path)
                created_any = True
            except OSError:
                pass

    return created_any


def _check_openeye_version():
    """Check that the OpenEye version matches what was used at build time."""
    try:
        from . import _build_info
    except ImportError:
        return

    if getattr(_build_info, 'OPENEYE_LIBRARY_TYPE', 'STATIC') != 'SHARED':
        return

    build_version = getattr(_build_info, 'OPENEYE_BUILD_VERSION', None)
    if not build_version:
        return

    try:
        from openeye import oechem
        runtime_version = oechem.OEToolkitsGetRelease()
        if runtime_version and build_version:
            build_parts = build_version.split('.')[:3]
            runtime_parts = runtime_version.split('.')[:3]
            if build_parts != runtime_parts:
                warnings.warn(
                    f"OpenEye version mismatch: oemaestro was built with OpenEye Toolkits "
                    f"{build_version} but runtime has OpenEye Toolkits {runtime_version}. "
                    f"This may cause compatibility issues.",
                    RuntimeWarning
                )
    except ImportError:
        warnings.warn(
            "openeye-toolkits package not found. "
            "This wheel requires openeye-toolkits to be installed. "
            "Install with: pip install openeye-toolkits",
            ImportWarning
        )


# Create compatibility symlinks before loading the C extension
_ensure_library_compat()

# Check OpenEye version on import
_check_openeye_version()

# Import SWIG-generated bindings
from .oemaestro import (
    # Enums
    TAG_NONE, TAG_TYPE, TAG_OWNER, TAG_NAME, TAG_ALL,
    PERCEPTION_NONE, PERCEPTION_CONNECTIVITY, PERCEPTION_RINGS,
    PERCEPTION_BOND_ORDERS, PERCEPTION_IMPLICIT_HYDROGENS,
    PERCEPTION_FORMAL_CHARGES, PERCEPTION_ALL,
    # Config
    OEMaestroReaderConfig,
    # IR types (low-level)
    MaestroAtom, MaestroBond, MaestroMol,
    # Layer 1-2
    MaestroReader, MolConverter,
    # Layer 3 C++ reader
    OEMaestroReader as _CppOEMaestroReader,
    # Single-mol read
    OEReadMaestro as _CppOEReadMaestro,
)


class OEMaestroReader:
    """High-level Python reader for Maestro format files.

    Wraps the C++ OEMaestroReader and provides Pythonic iteration.
    Each iteration yields an ``oechem.OEGraphMol`` populated from one CT block.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param config: Optional OEMaestroReaderConfig for tag format and perception.

    Example::

        from oemaestro import OEMaestroReader
        for mol in OEMaestroReader("input.mae"):
            print(mol.GetTitle(), mol.NumAtoms())
    """

    def __init__(self, source, config=None):
        if config is not None:
            self._reader = _CppOEMaestroReader(source, config)
        else:
            self._reader = _CppOEMaestroReader(source)

    def __iter__(self):
        return self

    def __next__(self):
        from openeye import oechem
        mol = oechem.OEGraphMol()
        if self._reader.Read(mol):
            return mol
        raise StopIteration

    def read(self, mol):
        """Read the next molecule into the provided OEMolBase.

        :param mol: An OpenEye OEMolBase-derived object to populate.
        :returns: True if a molecule was read, False at EOF.
        """
        return self._reader.Read(mol)

    def set_perception(self, perception):
        """Set the perception bitmask.

        :param perception: OEMaestroPerception bitmask value.
        """
        self._reader.SetPerception(perception)

    def set_tag_format(self, tags):
        """Set the tag format bitmask.

        :param tags: OEMaestroTag bitmask value.
        """
        self._reader.SetTagFormat(tags)

    def get_perception(self):
        """Get the current perception bitmask."""
        return self._reader.GetPerception()

    def get_tag_format(self):
        """Get the current tag format bitmask."""
        return self._reader.GetTagFormat()

    def get_config(self):
        """Get the current reader configuration."""
        return self._reader.GetConfig()

    def __repr__(self):
        config = self._reader.GetConfig()
        return f"OEMaestroReader(tags={config.tags}, perception={config.perception})"


def OEReadMaestro(source, mol=None, config=None):
    """Read molecules from a Maestro file.

    When called with a molecule argument, reads a single molecule into it
    and returns True/False. When called without a molecule, returns an
    OEMaestroReader for iteration.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param mol: Optional OEMolBase to populate (single-read mode).
    :param config: Optional OEMaestroReaderConfig.
    :returns: bool (single-read mode) or OEMaestroReader (iterator mode).

    Example::

        from openeye import oechem
        from oemaestro import OEReadMaestro

        # Single molecule
        mol = oechem.OEGraphMol()
        ok = OEReadMaestro("input.mae", mol)

        # Iterator
        for mol in OEReadMaestro("multi.mae"):
            print(mol.NumAtoms())
    """
    if mol is not None:
        if config is not None:
            return _CppOEReadMaestro(source, mol, config)
        return _CppOEReadMaestro(source, mol)
    return OEMaestroReader(source, config=config)


__all__ = [
    "__version__",
    "__version_info__",
    # Enums
    "TAG_NONE", "TAG_TYPE", "TAG_OWNER", "TAG_NAME", "TAG_ALL",
    "PERCEPTION_NONE", "PERCEPTION_CONNECTIVITY", "PERCEPTION_RINGS",
    "PERCEPTION_BOND_ORDERS", "PERCEPTION_IMPLICIT_HYDROGENS",
    "PERCEPTION_FORMAL_CHARGES", "PERCEPTION_ALL",
    # Config
    "OEMaestroReaderConfig",
    # IR types
    "MaestroAtom", "MaestroBond", "MaestroMol",
    # Layer 1-2
    "MaestroReader", "MolConverter",
    # Public API
    "OEMaestroReader",
    "OEReadMaestro",
]
