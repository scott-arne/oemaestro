"""Package import behavior tests."""

import importlib
import importlib.machinery
import shutil
import subprocess
import sys
from pathlib import Path


def test_import_does_not_load_openeye_runtime_modules(monkeypatch, tmp_path):
    """Importing oemaestro should not import openeye.libs or openeye.oechem."""
    package = "oemaestro"
    source_dir = tmp_path / package
    shutil.copytree(
        "python/oemaestro",
        source_dir,
        ignore=shutil.ignore_patterns(
            "__pycache__",
            "_*.so",
            "_*.pyd",
            "_*.dylib",
            "lib*.so",
            "lib*.dylib",
            "lib*.a",
        ),
    )

    (source_dir / "_build_info.py").write_text(
        "OPENEYE_LIBRARY_TYPE = 'SHARED'\n"
        "OPENEYE_EXPECTED_LIBS = ['liboechem-4.3.0.1.dylib']\n"
        "OPENEYE_BUILD_VERSION = '4.3.0.1'\n"
    )
    (source_dir / "oemaestro.py").write_text(
        "class _Stub:\n"
        "    def __init__(self, *args, **kwargs):\n"
        "        pass\n"
        "    def __call__(self, *args, **kwargs):\n"
        "        return None\n"
        "\n"
        "def __getattr__(name):\n"
        "    return _Stub\n"
    )

    fake_openeye = tmp_path / "openeye"
    fake_libs = fake_openeye / "libs"
    fake_runtime = fake_libs / "python3-osx-universal-clang++"
    fake_runtime.mkdir(parents=True)
    (fake_openeye / "__init__.py").write_text("")
    marker = tmp_path / "openeye_imported.txt"
    (fake_libs / "__init__.py").write_text(
        f"from pathlib import Path\nPath({str(marker)!r}).write_text('libs')\n"
    )
    (fake_openeye / "oechem.py").write_text(
        f"from pathlib import Path\nPath({str(marker)!r}).write_text('oechem')\n"
    )
    (fake_runtime / "liboechem-4.3.0.1.dylib").write_text("not a real library")

    for module_name in list(sys.modules):
        if module_name == package or module_name.startswith(f"{package}."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)
        if module_name == "openeye" or module_name.startswith("openeye."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)

    monkeypatch.setattr(
        sys,
        "meta_path",
        [
            finder
            for finder in sys.meta_path
            if package not in type(finder).__module__
        ],
    )
    monkeypatch.syspath_prepend(str(tmp_path))
    importlib.invalidate_caches()

    importlib.import_module(package)

    assert not marker.exists()
    assert "openeye.libs" not in sys.modules
    assert "openeye.oechem" not in sys.modules


def test_import_uses_user_cache_for_broken_openeye_runtime_compat_symlink(
    monkeypatch,
    tmp_path,
):
    """Broken OpenEye runtime symlinks should not mutate oemaestro."""
    package = "oemaestro"
    source_dir = tmp_path / package
    shutil.copytree(
        f"python/{package}",
        source_dir,
        ignore=shutil.ignore_patterns(
            "__pycache__",
            "_*.so",
            "_*.pyd",
            "_*.dylib",
            "lib*.so",
            "lib*.dylib",
            "lib*.a",
        ),
    )
    expected_name = "liboechem-4.3.0.1.so"
    runtime_name = "liboechem-4.3.0.3.so"

    (source_dir / "_build_info.py").write_text(
        "OPENEYE_LIBRARY_TYPE = 'SHARED'\n"
        f"OPENEYE_EXPECTED_LIBS = [{expected_name!r}]\n"
        "OPENEYE_BUILD_VERSION = '2025.2.1'\n"
    )
    (source_dir / f"{package}.py").write_text(
        'class _Stub:\n    def __init__(self, *args, **kwargs):\n        pass\n    def __call__(self, *args, **kwargs):\n        return None\n\ndef __getattr__(name):\n    return _Stub\n'
    )

    fake_openeye = tmp_path / "openeye"
    fake_libs = fake_openeye / "libs"
    fake_runtime = fake_libs / "python3-linux-x64-g++10.x"
    fake_runtime.mkdir(parents=True)
    (fake_openeye / "__init__.py").write_text("")
    marker = tmp_path / "openeye_imported.txt"
    (fake_libs / "__init__.py").write_text(
        f"from pathlib import Path\nPath({str(marker)!r}).write_text('libs')\n"
    )
    (fake_openeye / "oechem.py").write_text(
        f"from pathlib import Path\nPath({str(marker)!r}).write_text('oechem')\n"
    )
    (fake_runtime / runtime_name).write_text("not a real library")
    (fake_runtime / expected_name).symlink_to(fake_runtime / "missing-liboechem.so")
    cache_home = tmp_path / "cache"

    for module_name in list(sys.modules):
        if module_name == package or module_name.startswith(f"{package}."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)
        if module_name == "openeye" or module_name.startswith("openeye."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)

    monkeypatch.setattr(
        sys,
        "meta_path",
        [
            finder
            for finder in sys.meta_path
            if package not in type(finder).__module__
        ],
    )
    monkeypatch.syspath_prepend(str(tmp_path))
    monkeypatch.setenv("XDG_CACHE_HOME", str(cache_home))
    importlib.invalidate_caches()


    importlib.import_module(package)

    assert not marker.exists()
    assert "openeye.libs" not in sys.modules
    assert "openeye.oechem" not in sys.modules
    assert not (source_dir / expected_name).exists()
    cached_aliases = list(
        cache_home.glob(f"{package}/openeye-libs/**/{expected_name}")
    )
    assert len(cached_aliases) == 1
    assert cached_aliases[0].is_symlink()
    assert cached_aliases[0].resolve().name == runtime_name


def test_import_loads_extension_from_cache_when_openeye_aliases_are_needed(
    monkeypatch,
    tmp_path,
):
    """Alias-dependent imports should load the extension from the cache."""
    package = "oemaestro"
    source_dir = tmp_path / package
    shutil.copytree(
        f"python/{package}",
        source_dir,
        ignore=shutil.ignore_patterns(
            "__pycache__",
            "_*.so",
            "_*.pyd",
            "_*.dylib",
            "lib*.so",
            "lib*.dylib",
            "lib*.a",
        ),
    )
    expected_name = "liboegrid-4.3.0.1.dylib"
    dependency_name = "liboegrid-4.3.0.2.dylib"
    runtime_name = "liboegrid-4.3.0.3.dylib"
    extension_suffix = importlib.machinery.EXTENSION_SUFFIXES[0]

    (source_dir / "_build_info.py").write_text(
        "OPENEYE_LIBRARY_TYPE = 'SHARED'\n"
        f"OPENEYE_EXPECTED_LIBS = [{expected_name!r}]\n"
        "OPENEYE_BUILD_VERSION = '2025.2.1'\n"
    )
    (source_dir / "_oemaestro.py").unlink(missing_ok=True)
    (source_dir / f"_oemaestro{extension_suffix}").write_bytes(b"not a real dylib")
    (source_dir / f"{package}.py").write_text(
        "from . import _oemaestro\n"
        "EXTENSION_FILE = _oemaestro.__file__\n"
        "\n"
        "class _Stub:\n"
        "    def __init__(self, *args, **kwargs):\n"
        "        pass\n"
        "    def __call__(self, *args, **kwargs):\n"
        "        return None\n"
        "\n"
        "def __getattr__(name):\n"
        "    return _Stub\n"
    )

    fake_openeye = tmp_path / "openeye"
    fake_libs = fake_openeye / "libs"
    fake_runtime = fake_libs / "python3-osx-universal-clang++"
    fake_runtime.mkdir(parents=True)
    (fake_openeye / "__init__.py").write_text("")
    (fake_runtime / runtime_name).write_text("not a real library")
    (fake_runtime / expected_name).symlink_to(fake_runtime / "missing-liboegrid.dylib")
    cache_home = tmp_path / "cache"

    original_spec_from_file_location = importlib.util.spec_from_file_location

    class FakeExtensionLoader:
        def create_module(self, spec):
            return None

        def exec_module(self, module):
            module.__file__ = str(module.__spec__.origin)

    def fake_spec_from_file_location(name, location, *args, **kwargs):
        if name == f"{package}._oemaestro":
            spec = importlib.machinery.ModuleSpec(
                name=name,
                loader=FakeExtensionLoader(),
                origin=str(location),
            )
            spec.has_location = True
            return spec
        return original_spec_from_file_location(name, location, *args, **kwargs)

    original_subprocess_run = subprocess.run

    def fake_subprocess_run(command, *args, **kwargs):
        if command[:2] == ["otool", "-L"]:
            return subprocess.CompletedProcess(
                command,
                0,
                stdout=(
                    f"{command[2]}:\n"
                    f"\t@rpath/{dependency_name} "
                    "(compatibility version 0.0.0, current version 0.0.0)\n"
                ),
                stderr="",
            )
        return original_subprocess_run(command, *args, **kwargs)

    for module_name in list(sys.modules):
        if module_name == package or module_name.startswith(f"{package}."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)
        if module_name == "openeye" or module_name.startswith("openeye."):
            monkeypatch.delitem(sys.modules, module_name, raising=False)

    monkeypatch.setattr(
        sys,
        "meta_path",
        [
            finder
            for finder in sys.meta_path
            if package not in type(finder).__module__
        ],
    )
    monkeypatch.setattr(
        importlib.util,
        "spec_from_file_location",
        fake_spec_from_file_location,
    )
    monkeypatch.setattr(subprocess, "run", fake_subprocess_run)
    monkeypatch.syspath_prepend(str(tmp_path))
    monkeypatch.setenv("XDG_CACHE_HOME", str(cache_home))
    # Exercise the mocked Mach-O dependency path on every CI host OS.
    monkeypatch.setattr(sys, "platform", "darwin")
    importlib.invalidate_caches()

    importlib.import_module(package)

    cached_extension = Path(sys.modules[f"{package}.oemaestro"].EXTENSION_FILE)
    assert cached_extension.parent.is_dir()
    assert cache_home in cached_extension.parents
    assert cached_extension.name == f"_oemaestro{extension_suffix}"
    assert (cached_extension.parent / expected_name).resolve().name == runtime_name
    assert (cached_extension.parent / dependency_name).resolve().name == runtime_name
