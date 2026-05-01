"""Package import behavior tests."""

import importlib
import shutil
import sys


def test_import_does_not_load_openeye_runtime_modules(monkeypatch, tmp_path):
    """Importing oemaestro should not import openeye.libs or openeye.oechem."""
    package = "oemaestro"
    source_dir = tmp_path / package
    shutil.copytree("python/oemaestro", source_dir, ignore=shutil.ignore_patterns("__pycache__"))

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
