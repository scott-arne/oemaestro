#!/usr/bin/env python3
"""
Build and package oemaestro for PyPI distribution.

This script builds:
1. oemaestro: Binary wheel with compiled extensions

All packages are placed in dist/ ready for upload to PyPI.

Usage:
    python scripts/build_python.py [options]

Options:
    --openeye-root PATH    Path to OpenEye C++ SDK (headers)
    --python PATH          Python executable to use
    --clean                Clean dist/ before building
    --upload               Upload to PyPI after building (requires twine)
    --test-upload          Upload to TestPyPI instead of PyPI
    --verbose              Verbose output

Environment Variables:
    OPENEYE_ROOT    Path to OpenEye C++ SDK (alternative to --openeye-root)
    PYTHON          Python executable (alternative to --python)

If --openeye-root is not provided and neither OPENEYE_ROOT nor OE_DIR
environment variables are set, the script will attempt to read OPENEYE_ROOT
from CMakePresets.json or CMakeUserPresets.json in the project root.

Examples:
    # Build everything
    python scripts/build_python.py --openeye-root /path/to/openeye/sdk

    # Build and upload to TestPyPI
    python scripts/build_python.py --openeye-root /path/to/openeye/sdk --upload --test-upload
"""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


class Colors:
    """ANSI color codes for terminal output."""
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

    @classmethod
    def disable(cls):
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.NC = ''


def print_header(msg):
    print(f"\n{Colors.GREEN}{'=' * 60}{Colors.NC}")
    print(f"{Colors.GREEN}{msg}{Colors.NC}")
    print(f"{Colors.GREEN}{'=' * 60}{Colors.NC}\n")


def print_step(msg):
    print(f"{Colors.YELLOW}>>> {msg}{Colors.NC}")


def print_error(msg):
    print(f"{Colors.RED}ERROR: {msg}{Colors.NC}", file=sys.stderr)


def print_success(msg):
    print(f"{Colors.GREEN}{msg}{Colors.NC}")


def run_command(cmd, cwd=None, check=True, capture_output=False, verbose=False):
    """Run a command and optionally capture output."""
    if verbose:
        print(f"{Colors.BLUE}Running: {' '.join(cmd)}{Colors.NC}")

    result = subprocess.run(
        cmd,
        cwd=cwd,
        check=check,
        capture_output=capture_output,
        text=True
    )
    return result


def get_openeye_info(python_exe):
    """Get OpenEye toolkits version and library directory from Python."""
    code = """
from openeye import libs, oechem
import os
dll_dir = libs.FindOpenEyeDLLSDirectory()
version = oechem.OEToolkitsGetRelease()
print(f'VERSION:{version}')
print(f'LIB_DIR:{dll_dir}')
print(f'PLATFORM:{os.path.basename(dll_dir)}')
"""
    try:
        result = run_command(
            [python_exe, '-c', code],
            capture_output=True,
            check=True
        )
        info = {}
        for line in result.stdout.strip().split('\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                info[key] = value
        return info
    except subprocess.CalledProcessError as e:
        if e.stderr:
            print_step(f"openeye import error: {e.stderr.strip()}")
        return None


def verify_openeye_root(openeye_root):
    """Verify OpenEye C++ SDK path has headers."""
    include_dir = Path(openeye_root) / 'include'
    if not include_dir.exists():
        print_error(f"OpenEye include directory not found: {include_dir}")
        return False
    if not (include_dir / 'oechem.h').exists():
        print_error(f"oechem.h not found in {include_dir}")
        return False
    return True


def get_openeye_root_from_cmake_presets(project_dir: Path) -> str | None:
    """Read OPENEYE_ROOT from CMake preset files.

    :param project_dir: Root directory of the project.
    :returns: OPENEYE_ROOT path string, or None if not found.
    """
    all_presets: dict[str, dict] = {}

    for filename in ("CMakePresets.json", "CMakeUserPresets.json"):
        filepath = project_dir / filename
        if not filepath.exists():
            continue
        try:
            data = json.loads(filepath.read_text())
        except (json.JSONDecodeError, OSError):
            continue
        for preset in data.get("configurePresets", []):
            name = preset.get("name")
            if name:
                all_presets[name] = preset

    def resolve_cache_variables(preset_name: str, visited: set | None = None) -> dict:
        if visited is None:
            visited = set()
        if preset_name in visited or preset_name not in all_presets:
            return {}
        visited.add(preset_name)

        entry = all_presets[preset_name]
        inherits = entry.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]

        merged: dict = {}
        for parent_name in inherits:
            merged.update(resolve_cache_variables(parent_name, visited))
        merged.update(entry.get("cacheVariables", {}))
        return merged

    for hidden in (False, True):
        for name, preset in all_presets.items():
            if preset.get("hidden", False) != hidden:
                continue
            cache_vars = resolve_cache_variables(name)
            value = cache_vars.get("OPENEYE_ROOT")
            if value and isinstance(value, str):
                return value

    return None


def get_version_from_pyproject(pyproject_path):
    """Extract version string from a pyproject.toml file."""
    if not pyproject_path.exists():
        return None
    content = pyproject_path.read_text()
    match = re.search(r'version\s*=\s*"([^"]+)"', content)
    if match:
        return match.group(1)
    return None


def get_lib_version(project_dir):
    """Get the oemaestro version from pyproject.toml."""
    lib_pyproject = Path(project_dir) / 'pyproject.toml'
    lib_version = get_version_from_pyproject(lib_pyproject)
    if lib_version is None:
        print_error(f"Could not extract version from {lib_pyproject}")
        return None
    return lib_version


def build_oemaestro(project_dir, python_exe, openeye_root, openeye_info, verbose=False):
    """Build the oemaestro binary wheel."""
    print_header("Building oemaestro (binary wheel)")

    openeye_version = openeye_info['VERSION']
    openeye_lib_dir = openeye_info['LIB_DIR']

    print_step(f"OpenEye Toolkits version: {openeye_version}")
    print_step(f"OpenEye library directory: {openeye_lib_dir}")
    print_step(f"OpenEye C++ SDK: {openeye_root}")

    print_step("Building wheel with pip...")
    cmd = [
        python_exe, '-m', 'pip', 'wheel', '.',
        '--no-build-isolation',
        '--no-deps',
        '--wheel-dir', 'dist',
        f'-C', f'cmake.define.OPENEYE_ROOT={openeye_root}',
        f'-C', f'cmake.define.OPENEYE_LIB_DIR={openeye_lib_dir}',
        f'-C', f'cmake.define.OPENEYE_TOOLKITS_VERSION={openeye_version}',
        '-C', 'cmake.define.OPENEYE_USE_SHARED=ON',
        '-C', 'cmake.define.OEMAESTRO_BUILD_TESTS=OFF',
        '-C', 'logging.level=INFO',
    ]
    run_command(cmd, cwd=project_dir, verbose=verbose)

    wheels = list(Path(project_dir, 'dist').glob('oemaestro-*.whl'))
    if not wheels:
        print_error("No wheel file created")
        return None

    wheel_file = wheels[0]
    print_step(f"Wheel built: {wheel_file.name}")

    if platform.system() == 'Darwin':
        wheel_file = run_delocate(project_dir, python_exe, wheel_file, openeye_info, verbose)

    print_success(f"oemaestro wheel: {wheel_file}")
    return wheel_file


def run_delocate(project_dir, python_exe, wheel_file, openeye_info, verbose=False):
    """Run delocate to bundle non-OpenEye dependencies (macOS only)."""
    try:
        run_command([python_exe, '-c', 'import delocate'], capture_output=True, check=True)
    except subprocess.CalledProcessError:
        print_step("delocate not installed, skipping dependency bundling")
        return wheel_file

    print_step("Running delocate to bundle dependencies...")

    delocated_dir = Path(project_dir) / 'dist' / 'delocated'
    delocated_dir.mkdir(exist_ok=True)

    expected_missing = {'liboechem', 'liboesystem', 'liboeplatform', 'liboegraphsim',
                        'liboemath', 'libzstd', 'liboeshape', 'liboeff', 'liboegrid',
                        'liboespicoli', 'liboezap', 'liboequacpac', 'liboedepict',
                        'liboegrapheme', 'liboemolprop', 'liboeiupac', 'liboefastrocs'}

    try:
        result = subprocess.run([
            python_exe, '-m', 'delocate.cmd.delocate_wheel',
            '-w', str(delocated_dir),
            '-v',
            '--ignore-missing-dependencies',
            str(wheel_file)
        ], capture_output=True, text=True)

        if result.stdout and verbose:
            print(result.stdout)

        if result.stderr:
            lines = result.stderr.split('\n')
            i = 0
            while i < len(lines):
                line = lines[i].strip()
                i += 1
                if not line:
                    continue

                context = ' '.join(lines[max(0, i-1):min(len(lines), i+5)])
                is_expected = any(lib in context for lib in expected_missing)

                if is_expected:
                    while i < len(lines) and not lines[i].strip().startswith(('INFO:', 'ERROR:', 'WARNING:')):
                        i += 1
                    continue

                if line.startswith('INFO:delocate'):
                    if 'Copying' in line or 'Modifying' in line or 'Output:' in line:
                        print(f"  {line}")
                elif line.startswith('ERROR:') or line.startswith('WARNING:'):
                    print(f"  {line}")

        if result.returncode != 0:
            print_step(f"delocate returned non-zero exit code: {result.returncode}")

        delocated_wheels = list(delocated_dir.glob('*.whl'))
        if delocated_wheels:
            wheel_file.unlink()
            new_wheel = delocated_wheels[0]
            final_path = wheel_file.parent / new_wheel.name
            shutil.move(str(new_wheel), str(final_path))
            wheel_file = final_path

            print_step("Adding RPATH for openeye-toolkits libraries...")
            wheel_file = fix_rpath_and_sign(wheel_file, openeye_info)

    except Exception as e:
        print_step(f"delocate warning: {e}")

    if delocated_dir.exists():
        shutil.rmtree(delocated_dir, ignore_errors=True)

    return wheel_file


def fix_rpath_and_sign(wheel_file, openeye_info):
    """Fix RPATH and re-sign binary after delocate (macOS)."""
    import zipfile

    openeye_platform = openeye_info['PLATFORM']

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        with zipfile.ZipFile(wheel_file, 'r') as zf:
            zf.extractall(tmpdir)

        so_file = tmpdir / 'oemaestro' / '_oemaestro.so'
        if so_file.exists():
            try:
                subprocess.run([
                    'install_name_tool', '-add_rpath',
                    f'@loader_path/../openeye/libs/{openeye_platform}',
                    str(so_file)
                ], check=False, capture_output=True)

                subprocess.run([
                    'codesign', '-f', '-s', '-', str(so_file)
                ], check=False, capture_output=True)

                dylibs_dir = tmpdir / 'oemaestro' / '.dylibs'
                if dylibs_dir.exists():
                    for dylib in dylibs_dir.glob('*.dylib'):
                        subprocess.run([
                            'codesign', '-f', '-s', '-', str(dylib)
                        ], check=False, capture_output=True)

                print_step("RPATH added and binary re-signed")
            except Exception as e:
                print_step(f"Warning: Could not fix RPATH: {e}")

        wheel_file.unlink()
        with zipfile.ZipFile(wheel_file, 'w', zipfile.ZIP_DEFLATED) as zf:
            for file_path in tmpdir.rglob('*'):
                if file_path.is_file():
                    arcname = file_path.relative_to(tmpdir)
                    zf.write(file_path, arcname)

    return wheel_file


def upload_to_pypi(dist_dir, test_pypi=False, verbose=False):
    """Upload packages to PyPI using twine."""
    print_header(f"Uploading to {'TestPyPI' if test_pypi else 'PyPI'}")

    packages = list(Path(dist_dir).glob('oemaestro*.whl'))
    if not packages:
        print_error("No packages found to upload")
        return False

    print_step(f"Packages to upload:")
    for pkg in packages:
        print(f"  - {pkg.name}")

    cmd = ['twine', 'upload']
    if test_pypi:
        cmd.extend(['--repository', 'testpypi'])
    cmd.extend([str(p) for p in packages])

    try:
        run_command(cmd, verbose=verbose)
        print_success("Upload complete!")
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"Upload failed: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Build and package oemaestro for PyPI distribution",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--openeye-root',
        default=os.environ.get('OPENEYE_ROOT') or os.environ.get('OE_DIR'),
        help='Path to OpenEye C++ SDK (headers)'
    )
    parser.add_argument(
        '--python',
        default=os.environ.get('PYTHON', sys.executable),
        help='Python executable to use'
    )
    parser.add_argument(
        '--clean',
        action='store_true',
        help='Clean dist/ before building'
    )
    parser.add_argument(
        '--upload',
        action='store_true',
        help='Upload to PyPI after building'
    )
    parser.add_argument(
        '--test-upload',
        action='store_true',
        help='Upload to TestPyPI instead of PyPI'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output'
    )

    args = parser.parse_args()

    if args.no_color:
        Colors.disable()

    script_dir = Path(__file__).parent.resolve()
    project_dir = script_dir.parent

    print_header("OEMaestro Python Package Builder")
    print(f"Project directory: {project_dir}")
    print(f"Python: {args.python}")

    lib_version = get_lib_version(project_dir)
    if lib_version:
        print_step(f"oemaestro version: {lib_version}")

    dist_dir = project_dir / 'dist'
    if args.clean and dist_dir.exists():
        print_step("Cleaning dist/...")
        for f in dist_dir.glob('oemaestro*'):
            f.unlink()

    dist_dir.mkdir(exist_ok=True)

    openeye_info = get_openeye_info(args.python)
    if openeye_info:
        print(f"OpenEye Toolkits: {openeye_info['VERSION']}")
    else:
        print_step("openeye-toolkits not installed in Python environment")
        print_error("Cannot build oemaestro without openeye-toolkits")
        print("Install with: pip install openeye-toolkits")
        return 1

    built_packages = []

    if not args.openeye_root:
        preset_root = get_openeye_root_from_cmake_presets(project_dir)
        if preset_root:
            args.openeye_root = preset_root
            print_step(f"Using OPENEYE_ROOT from CMake presets: {preset_root}")
        else:
            print_error("OPENEYE_ROOT not set")
            print("Please set OPENEYE_ROOT environment variable, use --openeye-root,")
            print("or define it in CMakePresets.json / CMakeUserPresets.json")
            return 1

    if not verify_openeye_root(args.openeye_root):
        return 1

    wheel = build_oemaestro(
        project_dir,
        args.python,
        args.openeye_root,
        openeye_info,
        verbose=args.verbose
    )
    if wheel:
        built_packages.append(wheel)
    else:
        print_error("Failed to build oemaestro")
        return 1

    print_header("Build Summary")
    print("Built packages:")
    for pkg in built_packages:
        size_kb = pkg.stat().st_size / 1024
        print(f"  {pkg.name} ({size_kb:.1f} KB)")

    print(f"\nPackages are in: {dist_dir}")

    if args.upload:
        if not upload_to_pypi(dist_dir, test_pypi=args.test_upload, verbose=args.verbose):
            return 1

    print_header("Done!")
    print("To upload to PyPI:")
    print(f"  twine upload {dist_dir}/oemaestro*")
    print("\nTo upload to TestPyPI:")
    print(f"  twine upload --repository testpypi {dist_dir}/oemaestro*")

    return 0


if __name__ == '__main__':
    sys.exit(main())
