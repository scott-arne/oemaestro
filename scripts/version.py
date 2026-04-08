#!/usr/bin/env python3
"""Manage version numbers across the oemaestro project.

Supports full PEP 440 version specifications including pre-release
(alpha, beta, rc), post-release, and dev suffixes.

Provides commands to display, bump, and sync the version consistently
across CMakeLists.txt, Python packages, and the C++ umbrella header.

Usage::

    python scripts/version.py get
    python scripts/version.py bump patch
    python scripts/version.py bump minor
    python scripts/version.py bump major
    python scripts/version.py sync 1.0.0
    python scripts/version.py sync 1.0.0rc1
    python scripts/version.py sync 1.0.0-rc1 --yes
"""

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import rich_click as click
from rich.console import Console
from rich.table import Table
from rich import box

console = Console()

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# PEP 440 pre-release labels and their normalized forms
_PRE_LABELS = {
    "a": "a", "alpha": "a",
    "b": "b", "beta": "b",
    "c": "rc", "rc": "rc", "preview": "rc",
}

# Regex for full PEP 440 version parsing (accepts common non-normalized forms)
_PEP440_RE = re.compile(
    r"^(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)"
    r"(?:"
    r"[-.]?(?P<pre_label>a|alpha|b|beta|c|rc|preview)[-.]?(?P<pre_num>\d+)"
    r")?"
    r"(?:"
    r"[.]?(?:post)[-.]?(?P<post_num>\d+)"
    r")?"
    r"(?:"
    r"[.]?(?:dev)[-.]?(?P<dev_num>\d+)"
    r")?$",
    re.IGNORECASE,
)


# ============================================================================
# Version data model
# ============================================================================


@dataclass
class Version:
    """A PEP 440 version with base release and optional suffixes."""

    major: int
    minor: int
    patch: int
    pre: Optional[tuple[str, int]] = None    # ("a"|"b"|"rc", N)
    post: Optional[int] = None               # post-release number
    dev: Optional[int] = None                # dev-release number

    @property
    def base(self) -> str:
        """Return the MAJOR.MINOR.PATCH base version string."""
        return f"{self.major}.{self.minor}.{self.patch}"

    @property
    def full(self) -> str:
        """Return the full PEP 440 normalized version string."""
        v = self.base
        if self.pre is not None:
            v += f"{self.pre[0]}{self.pre[1]}"
        if self.post is not None:
            v += f".post{self.post}"
        if self.dev is not None:
            v += f".dev{self.dev}"
        return v

    @property
    def info_tuple(self) -> str:
        """Return the version_info tuple string (base integers only)."""
        return f"{self.major}, {self.minor}, {self.patch}"

    @property
    def is_release(self) -> bool:
        """Return True if this is a final release (no pre/post/dev suffix)."""
        return self.pre is None and self.post is None and self.dev is None

    def __str__(self) -> str:
        return self.full


def parse_version(version_str: str) -> Version:
    """Parse a version string into a Version object.

    Accepts PEP 440 versions and common variants with hyphens/dots as
    separators (e.g. ``0.5.1-rc1``, ``0.5.1.rc1``, ``0.5.1rc1``).
    Also handles comma-separated tuples (``0, 5, 1``) for reading
    ``__version_info__``.

    :param version_str: Version string in any supported format.
    :returns: Parsed Version object.
    :raises ValueError: If the string cannot be parsed.
    """
    raw = version_str.strip()

    # Handle comma-separated tuple format from __version_info__
    if "," in raw:
        parts = [p.strip() for p in raw.split(",")]
        if len(parts) < 3:
            raise ValueError(f"Cannot parse version: {version_str}")
        return Version(int(parts[0]), int(parts[1]), int(parts[2]))

    m = _PEP440_RE.match(raw)
    if not m:
        raise ValueError(f"Cannot parse version: {version_str}")

    pre = None
    if m.group("pre_label"):
        label = _PRE_LABELS[m.group("pre_label").lower()]
        pre = (label, int(m.group("pre_num")))

    post = int(m.group("post_num")) if m.group("post_num") else None
    dev = int(m.group("dev_num")) if m.group("dev_num") else None

    return Version(
        major=int(m.group("major")),
        minor=int(m.group("minor")),
        patch=int(m.group("patch")),
        pre=pre, post=post, dev=dev,
    )


# ============================================================================
# Version file definitions
# ============================================================================


@dataclass
class VersionLocation:
    """A file location containing a version string.

    :param base_only: If True, only the MAJOR.MINOR.PATCH base version is
        written (used for CMake, C++ headers that don't support PEP 440).
    """

    file: Path
    label: str
    pattern: str
    replacement: str
    extract: str
    base_only: bool = False

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

    def read_version_parsed(self) -> Optional[Version]:
        """Extract the current version as a parsed Version object.

        :returns: Version or None if not found or unparseable.
        """
        raw = self.read_version()
        if raw is None:
            return None
        try:
            return parse_version(raw)
        except ValueError:
            return None

    def update_version(self, ver: Version) -> bool:
        """Replace the version string in this file.

        :param ver: Version to write.
        :returns: True if the file was modified.
        """
        if not self.file.exists():
            return False
        content = self.file.read_text()
        version_str = ver.base if self.base_only else ver.full
        new = self.replacement.format(
            major=ver.major, minor=ver.minor, patch=ver.patch,
            version=version_str, info_tuple=ver.info_tuple,
        )
        new_content, count = re.subn(self.pattern, new, content, flags=re.MULTILINE)
        if count == 0:
            return False
        self.file.write_text(new_content)
        return True


def _locations() -> list[VersionLocation]:
    """Return all version locations in the project."""
    return [
        # C++ header defines (base version only — integers)
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (MAJOR)",
            pattern=r"#define OEMAESTRO_VERSION_MAJOR \d+",
            replacement="#define OEMAESTRO_VERSION_MAJOR {major}",
            extract=r"#define OEMAESTRO_VERSION_MAJOR (\d+)",
            base_only=True,
        ),
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (MINOR)",
            pattern=r"#define OEMAESTRO_VERSION_MINOR \d+",
            replacement="#define OEMAESTRO_VERSION_MINOR {minor}",
            extract=r"#define OEMAESTRO_VERSION_MINOR (\d+)",
            base_only=True,
        ),
        VersionLocation(
            file=PROJECT_ROOT / "include" / "oemaestro" / "oemaestro.h",
            label="C++ header (PATCH)",
            pattern=r"#define OEMAESTRO_VERSION_PATCH \d+",
            replacement="#define OEMAESTRO_VERSION_PATCH {patch}",
            extract=r"#define OEMAESTRO_VERSION_PATCH (\d+)",
            base_only=True,
        ),
        # CMakeLists.txt (base version only — CMake VERSION doesn't support PEP 440)
        VersionLocation(
            file=PROJECT_ROOT / "CMakeLists.txt",
            label="CMakeLists.txt",
            pattern=r"(project\(oemaestro VERSION )\d+\.\d+\.\d+",
            replacement=r"\g<1>{major}.{minor}.{patch}",
            extract=r"project\(oemaestro VERSION (\d+\.\d+\.\d+)",
            base_only=True,
        ),
        # pyproject.toml (root — wheel build, full PEP 440)
        VersionLocation(
            file=PROJECT_ROOT / "pyproject.toml",
            label="oemaestro pyproject.toml",
            pattern=r'(^version\s*=\s*")[^"]+(")',
            replacement=r'\g<1>{version}\g<2>',
            extract=r'^version\s*=\s*"([^"]+)"',
        ),
        # pyproject.toml (python/ — editable dev install, full PEP 440)
        VersionLocation(
            file=PROJECT_ROOT / "python" / "pyproject.toml",
            label="python/ pyproject.toml",
            pattern=r'(^version\s*=\s*")[^"]+(")',
            replacement=r'\g<1>{version}\g<2>',
            extract=r'^version\s*=\s*"([^"]+)"',
        ),
        # __init__.py __version__ (full PEP 440)
        VersionLocation(
            file=PROJECT_ROOT / "python" / "oemaestro" / "__init__.py",
            label="oemaestro __version__",
            pattern=r'(__version__\s*=\s*")[^"]+(")',
            replacement=r'\g<1>{version}\g<2>',
            extract=r'__version__\s*=\s*"([^"]+)"',
        ),
        # __init__.py __version_info__ (base version only — integer tuple)
        VersionLocation(
            file=PROJECT_ROOT / "python" / "oemaestro" / "__init__.py",
            label="oemaestro __version_info__",
            pattern=r"(__version_info__\s*=\s*\()[^)]+(\))",
            replacement=r"\g<1>{info_tuple}\g<2>",
            extract=r"__version_info__\s*=\s*\(([^)]+)\)",
            base_only=True,
        ),
    ]


def _get_canonical_version() -> Optional[Version]:
    """Read the canonical version from the root pyproject.toml.

    :returns: Version or None.
    """
    for loc in _locations():
        if loc.label == "oemaestro pyproject.toml":
            return loc.read_version_parsed()
    return None


def _update_all(locations: list[VersionLocation], ver: Version,
                dry_run: bool = False) -> Table:
    """Update all version locations and return a results table.

    :param locations: Version locations to update.
    :param ver: Version to write.
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
    table.add_column("Version", justify="center")
    table.add_column("Result", justify="center")

    for loc in locations:
        rel_path = str(loc.file.relative_to(PROJECT_ROOT))
        target_str = ver.base if loc.base_only else ver.full

        if not loc.file.exists():
            table.add_row(rel_path, loc.label, target_str,
                          "[yellow]skipped (not found)[/yellow]")
            continue

        if dry_run:
            table.add_row(rel_path, loc.label, target_str,
                          "[cyan]would update[/cyan]")
        else:
            ok = loc.update_version(ver)
            if ok:
                table.add_row(rel_path, loc.label, target_str,
                              "[green]updated[/green]")
            else:
                table.add_row(rel_path, loc.label, target_str,
                              "[red]pattern not matched[/red]")

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
            parsed = Version(int(components["MAJOR"]), int(components["MINOR"]),
                             int(components["PATCH"]))
            ver_display = parsed.base
            if canonical is None:
                status = "[dim]?[/dim]"
            elif parsed.base == canonical.base:
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
        parsed = loc.read_version_parsed()
        rel_path = str(loc.file.relative_to(PROJECT_ROOT))

        if parsed is None:
            status = "[yellow]not found[/yellow]"
            ver_display = "-"
            all_ok = False
        elif canonical is None:
            status = "[dim]?[/dim]"
            ver_display = parsed.full
        else:
            # Compare base for base_only locations, full for others
            if loc.base_only:
                matches = parsed.base == canonical.base
            else:
                matches = parsed.full == canonical.full
            if matches:
                status = "[green]ok[/green]"
                ver_display = parsed.base if loc.base_only else parsed.full
            else:
                status = "[red]mismatch[/red]"
                all_ok = False
                ver_display = f"[red]{parsed.base if loc.base_only else parsed.full}[/red]"

        table.add_row(rel_path, loc.label, ver_display, status)

    console.print()
    console.print(table)
    console.print()

    if canonical:
        console.print(f"  Canonical version: [bold]{canonical.full}[/bold]")
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

    Bumping always produces a final release (any pre/post/dev suffix is cleared).
    """
    canonical = _get_canonical_version()
    if canonical is None:
        console.print("[red]Could not read current version from pyproject.toml[/red]")
        sys.exit(1)

    old_version = canonical.full

    if part == "major":
        new = Version(canonical.major + 1, 0, 0)
    elif part == "minor":
        new = Version(canonical.major, canonical.minor + 1, 0)
    else:
        new = Version(canonical.major, canonical.minor, canonical.patch + 1)

    console.print()
    console.print(f"  Version bump: [bold red]{old_version}[/bold red] -> [bold green]{new.full}[/bold green]")
    console.print()

    table = _update_all(_locations(), new, dry_run=dry_run)
    console.print(table)
    console.print()

    if dry_run:
        console.print("  [yellow]Dry run — no files were modified.[/yellow]")
        console.print(f"  Run [bold]python scripts/version.py bump {part}[/bold] to apply.")
    else:
        console.print(f"  [green]Version bumped to {new.full} in all files.[/green]")
    console.print()


@cli.command()
@click.argument("version")
@click.option("--yes", "-y", is_flag=True, help="Skip confirmation prompt.")
@click.option("--dry-run", is_flag=True, help="Show what would change without writing files.")
def sync(version: str, yes: bool, dry_run: bool):
    """Set all version numbers to VERSION across the entire project.

    VERSION supports full PEP 440 specification:

    \b
      0.5.1          Final release
      0.5.1a1        Alpha pre-release
      0.5.1b1        Beta pre-release
      0.5.1rc1       Release candidate
      0.5.1-rc1      Release candidate (alternative separator)
      0.5.1.post1    Post-release
      0.5.1.dev1     Development release
      0.5.1rc1.dev2  Combined pre + dev

    For CMake and C++ files, only the base MAJOR.MINOR.PATCH is written.
    """
    try:
        ver = parse_version(version)
    except ValueError:
        console.print(f"[red]Invalid version format: {version}[/red]")
        console.print("  Expected PEP 440 version (e.g. 1.0.0, 1.0.0rc1, 1.0.0.post1)")
        sys.exit(1)

    locations = _locations()
    existing_files = sorted(set(
        str(loc.file.relative_to(PROJECT_ROOT))
        for loc in locations if loc.file.exists()
    ))

    console.print()

    if not dry_run and not yes:
        console.print(f"  [bold yellow]This will set the version to {ver.full} in "
                       f"{len(existing_files)} files.[/bold yellow]")
        if not ver.is_release:
            console.print(f"  [dim]Base version {ver.base} will be used for CMake/C++ files.[/dim]")
        console.print()
        console.print("  Files that will be modified:")
        for path in existing_files:
            console.print(f"    - {path}")
        console.print()
        if not click.confirm("  Proceed?"):
            console.print("\n  [dim]Aborted.[/dim]\n")
            sys.exit(0)
        console.print()

    table = _update_all(locations, ver, dry_run=dry_run)
    console.print(table)
    console.print()

    if dry_run:
        console.print("  [yellow]Dry run — no files were modified.[/yellow]")
        console.print(f"  Run [bold]python scripts/version.py sync {ver.full} --yes[/bold] to apply.")
    else:
        console.print(f"  [green]All versions set to {ver.full}.[/green]")
    console.print()


if __name__ == "__main__":
    cli()
