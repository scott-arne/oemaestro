#!/usr/bin/env python3
"""Manage version numbers across the oemaestro project.

Provides commands to display, bump, and sync the version consistently
across CMakeLists.txt, Python packages, and the C++ umbrella header.

Usage::

    python scripts/version.py get
    python scripts/version.py bump patch
    python scripts/version.py bump minor
    python scripts/version.py bump major
    python scripts/version.py sync 1.0.0
    python scripts/version.py sync 1.0.0 --yes
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import rich_click as click
from rich.console import Console
from rich.table import Table
from rich import box

console = Console()

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# ============================================================================
# Version file definitions
# ============================================================================


@dataclass
class VersionLocation:
    """A file location containing a version string."""

    file: Path
    label: str
    pattern: str
    replacement: str
    extract: str

    def read_version(self) -> Optional[str]:
        """Extract the current version string from this location.

        :returns: Version string or None if not found.
        """
        if not self.file.exists():
            return None
        content = self.file.read_text()
        match = re.search(self.extract, content, re.MULTILINE)
        if match:
            return match.group(1)
        return None

    def read_version_tuple(self) -> Optional[tuple[int, int, int]]:
        """Extract the current version as a parsed (major, minor, patch) tuple.

        :returns: Version tuple or None if not found or unparseable.
        """
        raw = self.read_version()
        if raw is None:
            return None
        try:
            return _parse_version(raw)
        except ValueError:
            return None

    def update_version(self, major: int, minor: int, patch: int) -> bool:
        """Replace the version string in this file.

        :param major: Major version number.
        :param minor: Minor version number.
        :param patch: Patch version number.
        :returns: True if the file was modified.
        """
        if not self.file.exists():
            return False
        content = self.file.read_text()
        new = self.replacement.format(major=major, minor=minor, patch=patch)
        new_content, count = re.subn(self.pattern, new, content, flags=re.MULTILINE)
        if count == 0:
            return False
        self.file.write_text(new_content)
        return True


def _locations() -> list[VersionLocation]:
    """Return all version locations in the project."""
    return [
        # C++ header defines
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (MAJOR)",
            pattern=r"#define OEMAESTRO_VERSION_MAJOR \d+",
            replacement="#define OEMAESTRO_VERSION_MAJOR {major}",
            extract=r"#define OEMAESTRO_VERSION_MAJOR (\d+)",
        ),
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (MINOR)",
            pattern=r"#define OEMAESTRO_VERSION_MINOR \d+",
            replacement="#define OEMAESTRO_VERSION_MINOR {minor}",
            extract=r"#define OEMAESTRO_VERSION_MINOR (\d+)",
        ),
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (PATCH)",
            pattern=r"#define OEMAESTRO_VERSION_PATCH \d+",
            replacement="#define OEMAESTRO_VERSION_PATCH {patch}",
            extract=r"#define OEMAESTRO_VERSION_PATCH (\d+)",
        ),
        # CMakeLists.txt
        VersionLocation(
            file=PROJECT_ROOT / "CMakeLists.txt",
            label="CMakeLists.txt",
            pattern=r"(project\(oemaestro VERSION )\d+\.\d+\.\d+",
            replacement=r"\g<1>{major}.{minor}.{patch}",
            extract=r"project\(oemaestro VERSION (\d+\.\d+\.\d+)",
        ),
        # pyproject.toml (root — wheel build)
        VersionLocation(
            file=PROJECT_ROOT / "pyproject.toml",
            label="oemaestro pyproject.toml",
            pattern=r'(^version\s*=\s*")[\d.]+(")',
            replacement=r'\g<1>{major}.{minor}.{patch}\g<2>',
            extract=r'^version\s*=\s*"([\d.]+)"',
        ),
        # pyproject.toml (python/ — editable dev install)
        VersionLocation(
            file=PROJECT_ROOT / "python" / "pyproject.toml",
            label="python/ pyproject.toml",
            pattern=r'(^version\s*=\s*")[\d.]+(")',
            replacement=r'\g<1>{major}.{minor}.{patch}\g<2>',
            extract=r'^version\s*=\s*"([\d.]+)"',
        ),
        # __init__.py __version__
        VersionLocation(
            file=PROJECT_ROOT / "python" / "oemaestro" / "__init__.py",
            label="oemaestro __version__",
            pattern=r'(__version__\s*=\s*")[\d.]+(")',
            replacement=r'\g<1>{major}.{minor}.{patch}\g<2>',
            extract=r'__version__\s*=\s*"([\d.]+)"',
        ),
        # __init__.py __version_info__
        VersionLocation(
            file=PROJECT_ROOT / "python" / "oemaestro" / "__init__.py",
            label="oemaestro __version_info__",
            pattern=r"(__version_info__\s*=\s*\()[\d, ]+(\))",
            replacement=r"\g<1>{major}, {minor}, {patch}\g<2>",
            extract=r"__version_info__\s*=\s*\(([\d, ]+)\)",
        ),
    ]


def _parse_version(version_str: str) -> tuple[int, int, int]:
    """Parse a version string into (major, minor, patch).

    Handles dotted strings ("0.1.0"), comma-separated tuples ("0, 1, 0"),
    and major.minor-only strings ("0.1").

    :param version_str: Version string in any supported format.
    :returns: Tuple of (major, minor, patch).
    :raises ValueError: If the string cannot be parsed.
    """
    parts = version_str.strip().replace(",", ".").replace(" ", "").split(".")
    parts = [p for p in parts if p]
    if len(parts) < 2:
        raise ValueError(f"Cannot parse version: {version_str}")
    major = int(parts[0])
    minor = int(parts[1])
    patch = int(parts[2]) if len(parts) > 2 else 0
    return major, minor, patch


def _format_version(v: tuple[int, int, int]) -> str:
    """Format a version tuple as MAJOR.MINOR.PATCH.

    :param v: Version tuple.
    :returns: Formatted version string.
    """
    return f"{v[0]}.{v[1]}.{v[2]}"


def _get_canonical_version() -> Optional[tuple[int, int, int]]:
    """Read the canonical version from the root pyproject.toml.

    :returns: Tuple of (major, minor, patch) or None.
    """
    for loc in _locations():
        if loc.label == "oemaestro pyproject.toml":
            return loc.read_version_tuple()
    return None


def _update_all(locations: list[VersionLocation], major: int, minor: int,
                patch: int, dry_run: bool = False) -> Table:
    """Update all version locations and return a results table.

    :param locations: Version locations to update.
    :param major: Major version number.
    :param minor: Minor version number.
    :param patch: Patch version number.
    :param dry_run: If True, do not write files.
    :returns: Rich Table with results.
    """
    table = Table(
        title="dry run" if dry_run else "updated files",
        box=box.ROUNDED,
        show_lines=False,
        title_style="bold yellow" if dry_run else "bold green",
        header_style="bold",
    )
    table.add_column("File", style="blue", max_width=45)
    table.add_column("Location", style="dim")
    table.add_column("Result", justify="center")

    for loc in locations:
        rel_path = str(loc.file.relative_to(PROJECT_ROOT))

        if not loc.file.exists():
            table.add_row(rel_path, loc.label, "[yellow]skipped (not found)[/yellow]")
            continue

        if dry_run:
            table.add_row(rel_path, loc.label, "[cyan]would update[/cyan]")
        else:
            ok = loc.update_version(major, minor, patch)
            if ok:
                table.add_row(rel_path, loc.label, "[green]updated[/green]")
            else:
                table.add_row(rel_path, loc.label, "[red]pattern not matched[/red]")

    return table


# ============================================================================
# CLI
# ============================================================================

click.rich_click.USE_RICH_MARKUP = True
click.rich_click.SHOW_ARGUMENTS = True
click.rich_click.GROUP_ARGUMENTS_OPTIONS = True
click.rich_click.STYLE_COMMANDS_TABLE_COLUMN_WIDTH_RATIO = (1, 2)


@click.group()
def cli():
    """Manage oemaestro version numbers across all project files."""


@cli.command()
def get():
    """Display the current version in all project files."""
    locations = _locations()

    table = Table(
        title="oemaestro version numbers",
        box=box.ROUNDED,
        show_lines=True,
        title_style="bold cyan",
        header_style="bold",
    )
    table.add_column("File", style="blue", max_width=40)
    table.add_column("Location", style="dim")
    table.add_column("Version", justify="center")
    table.add_column("Status", justify="center")

    canonical = _get_canonical_version()
    all_ok = True

    # Group C++ header defines into a single row
    cpp_header_locs = [loc for loc in locations if loc.label.startswith("C++ header")]
    other_locs = [loc for loc in locations if not loc.label.startswith("C++ header")]

    if cpp_header_locs:
        components = {}
        for loc in cpp_header_locs:
            key = loc.label.split("(")[1].rstrip(")")  # MAJOR, MINOR, PATCH
            components[key] = loc.read_version()

        rel_path = str(cpp_header_locs[0].file.relative_to(PROJECT_ROOT))
        if all(v is not None for v in components.values()):
            parsed = (int(components["MAJOR"]), int(components["MINOR"]), int(components["PATCH"]))
            ver_display = _format_version(parsed)
            if canonical is None:
                status = "[dim]?[/dim]"
            elif parsed == canonical:
                status = "[green]ok[/green]"
            else:
                status = "[red]mismatch[/red]"
                all_ok = False
                ver_display = f"[red]{ver_display}[/red]"
        else:
            ver_display = "-"
            status = "[yellow]not found[/yellow]"
            all_ok = False

        table.add_row(rel_path, "C++ header", ver_display, status)

    for loc in other_locs:
        parsed = loc.read_version_tuple()
        rel_path = str(loc.file.relative_to(PROJECT_ROOT))

        if parsed is None:
            status = "[yellow]not found[/yellow]"
            ver_display = "-"
            all_ok = False
        elif canonical is None:
            status = "[dim]?[/dim]"
            ver_display = _format_version(parsed)
        elif parsed == canonical:
            status = "[green]ok[/green]"
            ver_display = _format_version(parsed)
        else:
            status = "[red]mismatch[/red]"
            all_ok = False
            ver_display = f"[red]{_format_version(parsed)}[/red]"

        table.add_row(rel_path, loc.label, ver_display, status)

    console.print()
    console.print(table)
    console.print()

    if canonical:
        console.print(f"  Canonical version: [bold]{_format_version(canonical)}[/bold]")
    if all_ok:
        console.print("  [green]All version numbers are consistent.[/green]")
    else:
        console.print("  [red]Version numbers are out of sync![/red]")
    console.print()


@cli.command()
@click.argument("part", type=click.Choice(["major", "minor", "patch"]))
@click.option("--dry-run", is_flag=True, help="Show what would change without writing files.")
def bump(part: str, dry_run: bool):
    """Bump the version number across all project files.

    PART must be one of: major, minor, patch.
    """
    canonical = _get_canonical_version()
    if canonical is None:
        console.print("[red]Could not read current version from pyproject.toml[/red]")
        sys.exit(1)

    major, minor, patch = canonical
    old_version = _format_version(canonical)

    if part == "major":
        major += 1
        minor = 0
        patch = 0
    elif part == "minor":
        minor += 1
        patch = 0
    else:
        patch += 1

    new_version = f"{major}.{minor}.{patch}"

    console.print()
    console.print(f"  Version bump: [bold red]{old_version}[/bold red] -> [bold green]{new_version}[/bold green]")
    console.print()

    table = _update_all(_locations(), major, minor, patch, dry_run=dry_run)
    console.print(table)
    console.print()

    if dry_run:
        console.print("  [yellow]Dry run — no files were modified.[/yellow]")
        console.print(f"  Run [bold]python scripts/version.py bump {part}[/bold] to apply.")
    else:
        console.print(f"  [green]Version bumped to {new_version} in all files.[/green]")
    console.print()


@cli.command()
@click.argument("version")
@click.option("--yes", "-y", is_flag=True, help="Skip confirmation prompt.")
@click.option("--dry-run", is_flag=True, help="Show what would change without writing files.")
def sync(version: str, yes: bool, dry_run: bool):
    """Set all version numbers to VERSION across the entire project.

    VERSION must be in MAJOR.MINOR.PATCH format (e.g. 1.0.0).
    """
    try:
        major, minor, patch = _parse_version(version)
    except ValueError:
        console.print(f"[red]Invalid version format: {version}[/red]")
        console.print("  Expected MAJOR.MINOR.PATCH (e.g. 1.0.0)")
        sys.exit(1)

    target = f"{major}.{minor}.{patch}"
    locations = _locations()
    file_count = sum(1 for loc in locations if loc.file.exists())

    console.print()

    if not dry_run and not yes:
        console.print(f"  [bold yellow]This will set the version to {target} in "
                       f"{file_count} locations across {len(set(loc.file for loc in locations if loc.file.exists()))} files.[/bold yellow]")
        console.print()
        console.print("  Files that will be modified:")
        for path in sorted(set(
            str(loc.file.relative_to(PROJECT_ROOT))
            for loc in locations if loc.file.exists()
        )):
            console.print(f"    - {path}")
        console.print()
        if not click.confirm("  Proceed?"):
            console.print("\n  [dim]Aborted.[/dim]\n")
            sys.exit(0)
        console.print()

    table = _update_all(locations, major, minor, patch, dry_run=dry_run)
    console.print(table)
    console.print()

    if dry_run:
        console.print("  [yellow]Dry run — no files were modified.[/yellow]")
        console.print(f"  Run [bold]python scripts/version.py sync {target} --yes[/bold] to apply.")
    else:
        console.print(f"  [green]All versions set to {target}.[/green]")
    console.print()


if __name__ == "__main__":
    cli()
