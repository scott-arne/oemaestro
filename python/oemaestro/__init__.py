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
import hashlib
import importlib.machinery
import importlib.util
import os
import re
import shutil
import sys
import warnings
from importlib import metadata
from pathlib import Path

__version__ = "0.8.3"
__version_info__ = (0, 8, 3)


_OPENEYE_COMPAT_PRELOAD_PATHS: list[str] = []
_OPENEYE_COMPAT_EXTENSION_DIR: Path | None = None


def _user_cache_root():
    """Return the per-user cache root for OpenEye compatibility aliases."""
    cache_home = os.environ.get("XDG_CACHE_HOME")
    if cache_home:
        return Path(cache_home) / "oemaestro"
    return Path.home() / ".cache" / "oemaestro"


def _runtime_openeye_version():
    """Return the installed OpenEye toolkit distribution version if available."""
    try:
        return metadata.version("openeye-toolkits")
    except metadata.PackageNotFoundError:
        return "unknown"


def _cache_key(oe_lib_dir, expected_libs, build_version, runtime_version):
    """Build a stable cache key for one OpenEye runtime library set."""
    key_data = "\n".join(
        [
            os.path.realpath(oe_lib_dir),
            build_version or "unknown",
            runtime_version or "unknown",
            *sorted(expected_libs),
        ]
    )
    return hashlib.sha256(key_data.encode("utf-8")).hexdigest()[:16]


def _runtime_shared_library_names(lib_names):
    """Return filenames that can participate in runtime dynamic loading."""
    return [
        lib_name
        for lib_name in lib_names
        if ".so" in lib_name
        or lib_name.endswith(".dylib")
        or lib_name.endswith(".dll")
    ]


def _is_openeye_runtime_library_name(lib_name):
    """Return whether a dependency belongs to the OpenEye runtime set."""
    return lib_name.startswith("liboe") or lib_name.startswith("libzstd.")


def _find_openeye_runtime_lib_dir(expected_libs=()):
    """Find the OpenEye runtime library directory without importing oechem."""
    search_locations = []
    openeye_module = sys.modules.get("openeye")
    openeye_path = getattr(openeye_module, "__path__", None)
    if openeye_path is not None:
        search_locations.extend(openeye_path)

    if not search_locations:
        try:
            openeye_spec = importlib.util.find_spec("openeye")
        except (ImportError, ValueError):
            openeye_spec = None
        if (
            openeye_spec is not None
            and openeye_spec.submodule_search_locations is not None
        ):
            search_locations.extend(openeye_spec.submodule_search_locations)

    expected_libs = set(_runtime_shared_library_names(expected_libs or ()))
    fallback_dir = None
    for package_root in search_locations:
        libs_root = Path(package_root) / "libs"
        if not libs_root.is_dir():
            continue

        # Importing openeye.libs eagerly imports oechem in some environments.
        # The runtime libraries are shipped below openeye/libs, so filesystem
        # discovery preserves the fresh-import condition.
        for root, _, files in os.walk(libs_root):
            file_set = set(files)
            if expected_libs and expected_libs.intersection(file_set):
                return root
            if fallback_dir is None and any(
                ".dylib" in lib_name or ".so" in lib_name or ".dll" in lib_name
                for lib_name in files
            ):
                fallback_dir = root

    return fallback_dir


def _library_family(lib_name):
    """Return the stable library family name for a versioned shared library."""
    match = re.match(r"(lib\w+?)(-[\d.]+)?(\.[\d.]*\w+)$", lib_name)
    if match is None:
        return None
    return match.group(1)


def _candidate_runtime_libraries(oe_lib_dir, expected_name):
    """Find runtime libraries with the same family as an expected filename."""
    family = _library_family(expected_name)
    if family is None:
        return []
    candidates = []
    for file_name in os.listdir(oe_lib_dir):
        candidate_path = os.path.join(oe_lib_dir, file_name)
        if not os.path.isfile(candidate_path):
            continue
        if file_name.startswith(f"{family}-") or file_name.startswith(f"{family}."):
            candidates.append(candidate_path)
    return sorted(candidates)


def _compatible_library_path(oe_lib_dir, expected_name):
    """Return a runtime library path and whether it needs an expected-name alias."""
    exact_path = os.path.join(oe_lib_dir, expected_name)
    if os.path.isfile(exact_path):
        return exact_path, False

    candidates = _candidate_runtime_libraries(oe_lib_dir, expected_name)
    if len(candidates) != 1:
        candidate_names = ", ".join(os.path.basename(path) for path in candidates)
        raise ImportError(
            f"Could not find a compatible OpenEye runtime library for "
            f"{expected_name!r} in {oe_lib_dir!r}. "
            f"Candidates: {candidate_names or 'none'}."
        )
    return candidates[0], True


def _extension_runtime_library_names(pkg_dir):
    """Return OpenEye runtime library names recorded by the extension."""
    extension_path = _find_extension_module_path(pkg_dir)
    if extension_path is None:
        return []

    if sys.platform == "darwin":
        return _mach_o_runtime_library_names(extension_path)
    if sys.platform.startswith("linux"):
        return _elf_runtime_library_names(extension_path)
    return []


def _mach_o_runtime_library_names(extension_path):
    """Return OpenEye dylib dependencies recorded in a Mach-O extension."""
    import subprocess

    try:
        result = subprocess.run(
            ["otool", "-L", str(extension_path)],
            check=True,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, OSError, subprocess.CalledProcessError):
        return []

    dependencies = []
    for line in result.stdout.splitlines()[1:]:
        dependency = line.strip().split(" ", 1)[0]
        lib_name = os.path.basename(dependency)
        if _is_openeye_runtime_library_name(lib_name):
            dependencies.append(lib_name)
    return dependencies


def _elf_runtime_library_names(extension_path):
    """Return OpenEye shared-library dependencies recorded in an ELF extension."""
    import subprocess

    try:
        result = subprocess.run(
            ["readelf", "-d", str(extension_path)],
            check=True,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, OSError, subprocess.CalledProcessError):
        return []

    dependencies = []
    for match in re.finditer(r"Shared library: \[(?P<name>[^\]]+)\]", result.stdout):
        lib_name = match.group("name")
        if _is_openeye_runtime_library_name(lib_name):
            dependencies.append(lib_name)
    return dependencies


def _ensure_cache_alias(cache_dir, expected_name, target_path):
    """Create or refresh an expected-name symlink in the user cache."""
    alias_path = cache_dir / expected_name
    if alias_path.is_symlink():
        if alias_path.resolve() == Path(target_path).resolve():
            return alias_path
        alias_path.unlink()
    elif alias_path.exists():
        raise ImportError(
            f"Cannot create OpenEye compatibility alias {alias_path}: "
            "a non-symlink file already exists at that path."
        )

    try:
        alias_path.symlink_to(target_path)
    except OSError as exc:
        raise ImportError(
            f"Could not create OpenEye compatibility alias "
            f"{alias_path} -> {target_path}: {exc}"
        ) from exc
    return alias_path

def _default_num_threads():
    """Return min(2, cpu_count), falling back to 1."""
    ncpu = os.cpu_count() or 1
    return min(ncpu, 2)



def _ensure_library_compat():
    """Prepare compatibility aliases when OpenEye library filenames drift.

    When oemaestro is built with shared OpenEye libraries, the compiled extension
    records the exact versioned library filenames (e.g., liboechem-4.3.0.1.dylib).
    If the user upgrades openeye-toolkits, these filenames change and the dynamic
    linker fails to load the extension.

    This function creates expected-name aliases in a user-writable cache instead
    of mutating the installed package directory. When aliases are needed, the
    extension is later loaded from the same cache directory so its $ORIGIN lookup
    can find those aliases.
    """
    global _OPENEYE_COMPAT_EXTENSION_DIR, _OPENEYE_COMPAT_PRELOAD_PATHS

    _OPENEYE_COMPAT_PRELOAD_PATHS = []
    _OPENEYE_COMPAT_EXTENSION_DIR = None

    try:
        from . import _build_info
    except ImportError:
        return False

    if getattr(_build_info, 'OPENEYE_LIBRARY_TYPE', 'STATIC') != 'SHARED':
        return False

    expected_libs = set(_runtime_shared_library_names(
        getattr(_build_info, 'OPENEYE_EXPECTED_LIBS', [])
    ))
    expected_libs.update(_extension_runtime_library_names(os.path.dirname(__file__)))
    expected_libs = sorted(expected_libs)
    if not expected_libs:
        return False

    oe_lib_dir = _find_openeye_runtime_lib_dir(expected_libs)
    if oe_lib_dir is None:
        return False

    if not os.path.isdir(oe_lib_dir):
        return False

    build_version = getattr(_build_info, 'OPENEYE_BUILD_VERSION', None)
    runtime_version = _runtime_openeye_version()
    cache_dir = (
        _user_cache_root()
        / "openeye-libs"
        / _cache_key(oe_lib_dir, expected_libs, build_version, runtime_version)
    )

    preload_paths = []
    needs_cached_origin = False
    for expected_name in expected_libs:
        actual_path, needs_alias = _compatible_library_path(oe_lib_dir, expected_name)
        if needs_alias:
            try:
                cache_dir.mkdir(parents=True, exist_ok=True)
            except OSError as exc:
                raise ImportError(
                    f"Could not create OpenEye compatibility cache directory "
                    f"{cache_dir}: {exc}"
                ) from exc
            alias_path = _ensure_cache_alias(cache_dir, expected_name, actual_path)
            preload_paths.append(str(alias_path))
            needs_cached_origin = True
        else:
            preload_paths.append(actual_path)

    _OPENEYE_COMPAT_PRELOAD_PATHS = preload_paths
    if needs_cached_origin:
        _OPENEYE_COMPAT_EXTENSION_DIR = cache_dir

    return needs_cached_origin


def _extension_suffixes():
    """Return extension-module suffixes for the active Python interpreter."""
    return tuple(importlib.machinery.EXTENSION_SUFFIXES)


def _find_extension_module_path(pkg_dir):
    """Find the installed _oemaestro extension file."""
    for suffix in _extension_suffixes():
        candidate = Path(pkg_dir) / f"_oemaestro{suffix}"
        if candidate.is_file():
            return candidate
    for candidate in Path(pkg_dir).glob("_oemaestro*"):
        if candidate.is_file() and str(candidate).endswith(_extension_suffixes()):
            return candidate
    return None


def _copy_if_stale(source_path, target_path):
    """Copy a file into the cache when size or mtime changed."""
    if (
        target_path.exists()
        and target_path.stat().st_size == source_path.stat().st_size
        and target_path.stat().st_mtime_ns == source_path.stat().st_mtime_ns
    ):
        return
    shutil.copy2(source_path, target_path)


def _copy_package_shared_sidecars(pkg_dir, cache_dir, extension_path):
    """Copy package-local shared library sidecars needed by cached extension."""
    for candidate in Path(pkg_dir).iterdir():
        name = candidate.name
        if not candidate.is_file() or candidate == extension_path:
            continue
        if (
            ".so" not in name
            and not name.endswith(".dylib")
            and not name.endswith(".dll")
            and not name.endswith(".pyd")
        ):
            continue
        _copy_if_stale(candidate, cache_dir / name)


def _load_cached_extension_if_needed():
    """Load _oemaestro from the cache when OpenEye aliases live there."""
    cache_dir = _OPENEYE_COMPAT_EXTENSION_DIR
    if cache_dir is None:
        return

    module_name = f"{__name__}._oemaestro"
    if module_name in sys.modules:
        return

    pkg_dir = os.path.dirname(__file__)
    extension_path = _find_extension_module_path(pkg_dir)
    if extension_path is None:
        return

    cached_extension_path = cache_dir / extension_path.name
    try:
        cache_dir.mkdir(parents=True, exist_ok=True)
        _copy_if_stale(extension_path, cached_extension_path)
        _copy_package_shared_sidecars(pkg_dir, cache_dir, extension_path)
    except OSError as exc:
        raise ImportError(
            f"Could not prepare cached oemaestro extension in {cache_dir}: {exc}"
        ) from exc

    spec = importlib.util.spec_from_file_location(module_name, cached_extension_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not create import spec for {cached_extension_path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(module_name, None)
        raise


def _preload_shared_libs():
    """Preload OpenEye shared libraries so the C extension can find them.

    On Linux, the extension's RUNPATH (set at build time) normally handles
    dependency resolution, but preloading ensures libraries are available
    even if RUNPATH is stripped (e.g. by certain packaging tools).
    On macOS, @rpath references may not resolve without preloading.

    Only the libraries recorded in ``OPENEYE_EXPECTED_LIBS`` are loaded,
    and they are loaded with ``RTLD_GLOBAL`` so that cross-module C++
    symbol references resolve correctly. Loading the entire OpenEye
    library directory (which can contain 70+ unrelated shared objects)
    would pollute the global symbol namespace and cause segfaults in
    unrelated C extensions such as ``_sqlite3``.
    """
    import ctypes
    import sys
    if sys.platform not in ('linux', 'darwin'):
        return

    try:
        from . import _build_info
    except ImportError:
        return

    if getattr(_build_info, 'OPENEYE_LIBRARY_TYPE', 'STATIC') != 'SHARED':
        return

    expected_libs = _runtime_shared_library_names(
        getattr(_build_info, 'OPENEYE_EXPECTED_LIBS', [])
    )
    if not expected_libs:
        return

    oe_lib_dir = _find_openeye_runtime_lib_dir(expected_libs)
    if oe_lib_dir is None:
        return

    if not os.path.isdir(oe_lib_dir):
        return

    paths = _OPENEYE_COMPAT_PRELOAD_PATHS
    if not paths:
        paths = [
            os.path.join(oe_lib_dir, lib_name)
            for lib_name in expected_libs
            if os.path.exists(os.path.join(oe_lib_dir, lib_name))
        ]

    for path in paths:
        if os.path.exists(path) or os.path.islink(path):
            try:
                ctypes.CDLL(path, mode=ctypes.RTLD_GLOBAL)
            except OSError:
                pass

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
        from importlib import metadata
        runtime_version = metadata.version("openeye-toolkits")
    except metadata.PackageNotFoundError:
        warnings.warn(
            "openeye-toolkits package not found. "
            "This wheel requires openeye-toolkits to be installed. "
            "Install with: pip install openeye-toolkits",
            ImportWarning
        )
        return

    build_parts = build_version.split('.')[:2]
    runtime_parts = runtime_version.split('.')[:2]
    if build_parts != runtime_parts:
        warnings.warn(
            f"OpenEye version mismatch: oemaestro was built with OpenEye Toolkits "
            f"{build_version} but runtime has OpenEye Toolkits {runtime_version}. "
            f"This may cause compatibility issues.",
            RuntimeWarning
        )


# Create compatibility symlinks before loading the C extension
_ensure_library_compat()

# Preload OpenEye shared libraries (needed on Linux where RUNPATH may not
# include the OpenEye library directory after auditwheel repair)
_preload_shared_libs()


def _preload_bundled_libs():
    """Preload libraries bundled by auditwheel from the .libs directory.

    auditwheel repair bundles non-manylinux dependencies (e.g. libbz2,
    ICU libraries from boost) into an ``oemaestro.libs/`` directory next
    to the package. The bundled copies have hashed filenames and must be
    loaded before the C extension to satisfy its DT_NEEDED entries.

    Libraries may have inter-dependencies (e.g. libicui18n depends on
    libicuuc which depends on libicudata), so we do multiple passes
    until no new libraries can be loaded.
    """
    import sys
    if sys.platform != 'linux':
        return

    import ctypes
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    site_dir = os.path.dirname(pkg_dir)
    for libs_name in ('oemaestro.libs', '.oemaestro.libs'):
        libs_dir = os.path.join(site_dir, libs_name)
        if not os.path.isdir(libs_dir):
            continue
        remaining = [
            os.path.join(libs_dir, f)
            for f in sorted(os.listdir(libs_dir))
            if '.so' in f
        ]
        # Multi-pass: keep retrying until no progress (handles dep ordering)
        while remaining:
            failed = []
            for lib_path in remaining:
                try:
                    ctypes.CDLL(lib_path)
                except OSError:
                    failed.append(lib_path)
            if len(failed) == len(remaining):
                break  # No progress, stop
            remaining = failed


_preload_bundled_libs()

# Load the extension from the alias cache when $ORIGIN must see cached aliases
_load_cached_extension_if_needed()

# Check OpenEye version on import
_check_openeye_version()

# Import SWIG-generated bindings
from .oemaestro import (
    # Enums
    TAG_NONE, TAG_TYPE, TAG_OWNER, TAG_NAME, TAG_ALL,
    PERCEPTION_NONE, PERCEPTION_CONNECTIVITY, PERCEPTION_RINGS,
    PERCEPTION_BOND_ORDERS, PERCEPTION_IMPLICIT_HYDROGENS,
    PERCEPTION_FORMAL_CHARGES, PERCEPTION_ALL, PERCEPTION_DEFAULT,
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
    # DU reader
    OEMaestroDesignUnitReader as _CppOEMaestroDesignUnitReader,
    OEReadMaestroDesignUnit as _CppOEReadMaestroDesignUnit,
    # Write mode enum
    WRITE_CREATE, WRITE_APPEND,
    # Writer config
    OEMaestroWriterConfig,
    # Tag converter
    OEMaestroTagConverter,
    # Layer 1 writer
    MaestroWriter,
    # Layer 3 C++ writer
    OEMaestroWriter as _CppOEMaestroWriter,
    # Single-mol write
    OEWriteMaestro as _CppOEWriteMaestro,
)


class OEMaestroReader:
    """High-level Python reader for Maestro format files.

    Wraps the C++ OEMaestroReader and provides Pythonic iteration.
    Each iteration yields an ``oechem.OEGraphMol`` populated from one CT block.
    When a conformer test is set via ``set_conf_test``, consecutive matching
    CTs are grouped into multi-conformer ``OEMol`` objects.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param config: Optional OEMaestroReaderConfig for tag format and perception.
    :param num_threads: Number of threads for parallel reading (default min(2, cpu_count)).

    Example::

        from oemaestro import OEMaestroReader
        for mol in OEMaestroReader("input.mae"):
            print(mol.GetTitle(), mol.NumAtoms())
    """

    def __init__(self, source, config=None, num_threads=None):
        from openeye import oechem  # type: ignore[import-untyped]
        self._oechem = oechem
        if num_threads is not None:
            if config is None:
                config = OEMaestroReaderConfig()
            config.SetNumThreads(num_threads)
        if config is not None:
            self._reader = _CppOEMaestroReader(source, config)
        else:
            self._reader = _CppOEMaestroReader(source)
        self._conf_test = None
        self._pending = None

    def __iter__(self):
        return self

    def __next__(self):
        if self._conf_test is None:
            mol = self._oechem.OEGraphMol()
            if self._reader.Read(mol):
                return mol
            raise StopIteration

        # Conformer grouping mode
        oechem = self._oechem
        if self._pending is not None:
            mol = self._pending
            self._pending = None
        else:
            ct = oechem.OEGraphMol()
            if not self._reader.Read(ct):
                raise StopIteration
            mol = oechem.OEMol(ct)

        # Lookahead: group consecutive matching CTs as conformers
        while True:
            ct = oechem.OEGraphMol()
            if not self._reader.Read(ct):
                break
            if self._conf_test.CompareMols(mol, ct):
                mol.NewConf(ct)
            else:
                self._pending = oechem.OEMol(ct)
                break

        return mol

    def read(self, mol):
        """Read the next molecule into the provided OEMolBase.

        :param mol: An OpenEye OEMolBase-derived object to populate.
        :returns: True if a molecule was read, False at EOF.
        """
        return self._reader.Read(mol)

    def set_conf_test(self, conf_test):
        """Set a conformer test for grouping CT blocks.

        When set, consecutive CTs that match under the test are grouped as
        conformers in a single OEMol. Pass None to disable grouping.

        :param conf_test: An OEConfTestBase object, or None to disable.
        """
        self._conf_test = conf_test
        self._pending = None

    def get_perception(self):
        """Get the current perception bitmask."""
        return self._reader.GetPerception()

    def get_tag_format(self):
        """Get the current tag format bitmask."""
        return self._reader.GetTagFormat()

    def get_config(self):
        """Get the current reader configuration."""
        return self._reader.GetConfig()

    def close(self):
        """Release the underlying C++ reader. Idempotent."""
        self._reader = None
        self._pending = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
        return False

    def __repr__(self):
        config = self._reader.GetConfig()
        return f"OEMaestroReader(tags={config.GetTags()}, perception={config.GetPerception()})"


def OEReadMaestro(source, mol=None, config=None, num_threads=None):
    """Read molecules from a Maestro file.

    When called with a molecule argument, reads a single molecule into it
    and returns True/False. When called without a molecule, returns an
    OEMaestroReader for iteration.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param mol: Optional OEMolBase to populate (single-read mode).
    :param config: Optional OEMaestroReaderConfig.
    :param num_threads: Number of threads for parallel reading (default min(2, cpu_count)).
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
    if num_threads is not None:
        if config is None:
            config = OEMaestroReaderConfig()
        config.SetNumThreads(num_threads)
    if mol is not None:
        if config is not None:
            return _CppOEReadMaestro(source, mol, config)
        return _CppOEReadMaestro(source, mol)
    return OEMaestroReader(source, config=config)


class OEMaestroDesignUnitReader:
    """High-level Python reader for Maestro files as design units.

    Wraps the C++ OEMaestroDesignUnitReader and provides Pythonic iteration.
    Each iteration yields an ``oechem.OEDesignUnit`` populated from one CT block.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param config: Optional OEMaestroReaderConfig for tag format and perception.
    :param num_threads: Number of threads for parallel reading (default min(2, cpu_count)).

    Example::

        from oemaestro import OEMaestroDesignUnitReader
        for du in OEMaestroDesignUnitReader("prepared.mae"):
            protein = oechem.OEGraphMol()
            du.GetProtein(protein)
            print(protein.NumAtoms())
    """

    def __init__(self, source, config=None, num_threads=None):
        from openeye import oechem
        self._oechem = oechem
        if num_threads is not None:
            if config is None:
                config = OEMaestroReaderConfig()
            config.SetNumThreads(num_threads)
        if config is not None:
            self._reader = _CppOEMaestroDesignUnitReader(source, config)
        else:
            self._reader = _CppOEMaestroDesignUnitReader(source)

    def __iter__(self):
        return self

    def __next__(self):
        du = self._oechem.OEDesignUnit()
        if self._reader.Read(du):
            return du
        raise StopIteration

    def read(self, du):
        """Read the next design unit into the provided OEDesignUnit.

        :param du: An OpenEye OEDesignUnit object to populate.
        :returns: True if a design unit was read, False at EOF.
        """
        return self._reader.Read(du)

    def set_ligand_predicate(self, pred):
        """Override the default ligand atom predicate.

        :param pred: An OpenEye OEUnaryAtomPred object.
        """
        self._reader.SetLigandPredicate(pred)

    def set_solvent_predicate(self, pred):
        """Override the default solvent atom predicate.

        :param pred: An OpenEye OEUnaryAtomPred object.
        """
        self._reader.SetSolventPredicate(pred)

    def set_cofactor_predicate(self, pred):
        """Override the default cofactor atom predicate.

        :param pred: An OpenEye OEUnaryAtomPred object.
        """
        self._reader.SetCofactorPredicate(pred)

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
        return f"OEMaestroDesignUnitReader(tags={config.GetTags()}, perception={config.GetPerception()})"


def OEReadMaestroDesignUnit(source, du=None, config=None, num_threads=None):
    """Read design units from a Maestro file.

    When called with a design unit argument, reads a single DU into it
    and returns True/False. When called without, returns an
    OEMaestroDesignUnitReader for iteration.

    :param source: Path to a Maestro file (.mae, .mae.gz, .maegz).
    :param du: Optional OEDesignUnit to populate (single-read mode).
    :param config: Optional OEMaestroReaderConfig.
    :param num_threads: Number of threads for parallel reading (default min(2, cpu_count)).
    :returns: bool (single-read mode) or OEMaestroDesignUnitReader (iterator mode).

    Example::

        from openeye import oechem
        from oemaestro import OEReadMaestroDesignUnit

        # Single design unit
        du = oechem.OEDesignUnit()
        ok = OEReadMaestroDesignUnit("prepared.mae", du)

        # Iterator
        for du in OEReadMaestroDesignUnit("multi.mae"):
            print(du.HasLigand())
    """
    if num_threads is not None:
        if config is None:
            config = OEMaestroReaderConfig()
        config.SetNumThreads(num_threads)
    if du is not None:
        if config is not None:
            return _CppOEReadMaestroDesignUnit(source, du, config)
        return _CppOEReadMaestroDesignUnit(source, du)
    return OEMaestroDesignUnitReader(source, config=config)


# --- Writer ---


class OEMaestroWriter:
    """High-level Maestro file writer with context manager support.

    :param source: Output filename (str or Path) or oeofstream.
    :param config: Optional OEMaestroWriterConfig.
    :param mode: Optional write mode (WRITE_CREATE or WRITE_APPEND).
    """

    def __init__(self, source, config=None, mode=None):
        from pathlib import Path
        if isinstance(source, Path):
            source = str(source)

        if config is not None:
            self._writer = _CppOEMaestroWriter(source, config)
        elif mode is not None:
            self._writer = _CppOEMaestroWriter(source, mode)
        else:
            self._writer = _CppOEMaestroWriter(source)

        self._closed = False

    def write(self, mol):
        """Write a molecule. Multi-conformer mols emit one CT per conformer.

        :param mol: An OpenEye OEMolBase (OEGraphMol or OEMol).
        :returns: True on success.
        """
        return self._writer.Write(mol)

    def close(self):
        """Flush and close the writer."""
        if not self._closed:
            self._writer.Close()
            self._closed = True

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


def OEWriteMaestro(source, mol, config=None):
    """Write a single molecule to a Maestro file.

    :param source: Output filename (str or Path) or oeofstream.
    :param mol: An OpenEye OEMolBase.
    :param config: Optional OEMaestroWriterConfig.
    :returns: True on success.
    """
    from pathlib import Path
    if isinstance(source, Path):
        source = str(source)
    if config is not None:
        return _CppOEWriteMaestro(source, mol, config)
    return _CppOEWriteMaestro(source, mol)


def _register_oeio_handler():
    """Register oemaestro as an oeio plugin handler if oeio is available.

    This enables ``oeio.read()`` and ``oeio.write()`` to handle Maestro
    format files transparently.
    """
    try:
        import oeio  # type: ignore[import-untyped]
    except ImportError:
        return

    if not hasattr(oeio, 'register_handler'):
        return

    def _maestro_reader(path):
        """Create an oeio-compatible reader for Maestro files."""
        return OEMaestroReader(path)

    class _MaestroWriterAdapter:
        """Adapter to match oeio's writer context-manager protocol."""

        def __init__(self, path):
            self._writer = OEMaestroWriter(path)

        def append(self, mol):
            return self._writer.write(mol)

        def close(self):
            self._writer.close()

        def __enter__(self):
            return self

        def __exit__(self, *args):
            self.close()
            return False

    oeio.register_handler(
        name="Maestro",
        extensions=[".mae", ".mae.gz", ".maegz"],
        description="Schrodinger Maestro format",
        reader_factory=_maestro_reader,
        writer_factory=_MaestroWriterAdapter,
    )


_register_oeio_handler()


__all__ = [
    "__version__",
    "__version_info__",
    # Enums
    "TAG_NONE", "TAG_TYPE", "TAG_OWNER", "TAG_NAME", "TAG_ALL",
    "PERCEPTION_NONE", "PERCEPTION_CONNECTIVITY", "PERCEPTION_RINGS",
    "PERCEPTION_BOND_ORDERS", "PERCEPTION_IMPLICIT_HYDROGENS",
    "PERCEPTION_FORMAL_CHARGES", "PERCEPTION_ALL", "PERCEPTION_DEFAULT",
    "WRITE_CREATE", "WRITE_APPEND",
    # Config
    "OEMaestroReaderConfig", "OEMaestroWriterConfig",
    # Tag converter
    "OEMaestroTagConverter",
    # IR types
    "MaestroAtom", "MaestroBond", "MaestroMol",
    # Layer 1-2
    "MaestroReader", "MaestroWriter", "MolConverter",
    # Public API
    "OEMaestroReader",
    "OEReadMaestro",
    "OEMaestroWriter",
    "OEWriteMaestro",
    "OEMaestroDesignUnitReader",
    "OEReadMaestroDesignUnit",
]
