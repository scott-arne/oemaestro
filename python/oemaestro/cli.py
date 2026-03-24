"""Command-line interface for oemaestro.

Converts Maestro format files (.mae, .mae.gz, .maegz) to OpenEye-supported
molecular formats and vice versa.
"""
import sys
from pathlib import Path

import rich_click as click

from oemaestro import (
    __version__,
    OEMaestroReader,
    OEMaestroReaderConfig,
    TAG_NONE, TAG_TYPE, TAG_OWNER, TAG_NAME, TAG_ALL,
    PERCEPTION_NONE, PERCEPTION_CONNECTIVITY, PERCEPTION_RINGS,
    PERCEPTION_BOND_ORDERS, PERCEPTION_IMPLICIT_HYDROGENS,
    PERCEPTION_FORMAL_CHARGES, PERCEPTION_ALL,
)

click.rich_click.USE_RICH_MARKUP = True
click.rich_click.GROUP_ARGUMENTS_OPTIONS = True
click.rich_click.SHOW_ARGUMENTS = True
click.rich_click.STYLE_ERRORS_SUGGESTION = "dim"

MAESTRO_EXTENSIONS = {".mae", ".maegz", ".mae.gz"}

SUPPORTED_OUTPUT_EXTENSIONS = {
    ".sdf": "SD file",
    ".mol2": "Tripos Mol2",
    ".pdb": "Protein Data Bank",
    ".smi": "SMILES",
    ".ism": "Isomeric SMILES",
    ".oeb": "OpenEye binary",
    ".oeb.gz": "OpenEye binary (compressed)",
    ".xyz": "XYZ coordinates",
    ".csv": "CSV",
    ".mol": "MDL Molfile",
    ".can": "Canonical SMILES",
}

TAG_CHOICES = {
    "all": TAG_ALL,
    "none": TAG_NONE,
    "name": TAG_NAME,
    "type": TAG_TYPE,
    "owner": TAG_OWNER,
}

PERCEPTION_CHOICES = {
    "all": PERCEPTION_ALL,
    "none": PERCEPTION_NONE,
    "connectivity": PERCEPTION_CONNECTIVITY,
    "rings": PERCEPTION_RINGS,
    "bond-orders": PERCEPTION_BOND_ORDERS,
    "implicit-h": PERCEPTION_IMPLICIT_HYDROGENS,
    "formal-charges": PERCEPTION_FORMAL_CHARGES,
}

CONF_TEST_CHOICES = ["none", "default", "isomeric", "absolute", "abs-canonical"]


def _get_file_ext(path):
    """Get the full extension, handling double extensions like .mae.gz."""
    name = Path(path).name.lower()
    for ext in (".mae.gz", ".oeb.gz"):
        if name.endswith(ext):
            return ext
    return Path(path).suffix.lower()


def _is_maestro_input(path):
    return _get_file_ext(path) in MAESTRO_EXTENSIONS


def _build_conf_test(name):
    """Build an OEConfTestBase from a string name."""
    from openeye import oechem
    if name == "none":
        return None
    elif name == "default":
        return oechem.OEDefaultConfTest()
    elif name == "isomeric":
        return oechem.OEIsomericConfTest()
    elif name == "absolute":
        return oechem.OEAbsoluteConfTest()
    elif name == "abs-canonical":
        return oechem.OEAbsCanonicalConfTest()
    return None


def _parse_tag_flags(values):
    """Parse tag flag strings into a bitmask."""
    if not values:
        return TAG_ALL
    result = TAG_NONE
    for v in values:
        if v in TAG_CHOICES:
            if v == "all":
                return TAG_ALL
            if v == "none":
                return TAG_NONE
            result |= TAG_CHOICES[v]
    return result


def _parse_perception_flags(values):
    """Parse perception flag strings into a bitmask."""
    if not values:
        return PERCEPTION_ALL
    result = PERCEPTION_NONE
    for v in values:
        if v in PERCEPTION_CHOICES:
            if v == "all":
                return PERCEPTION_ALL
            if v == "none":
                return PERCEPTION_NONE
            result |= PERCEPTION_CHOICES[v]
    return result


@click.group(invoke_without_command=True)
@click.version_option(__version__, prog_name="oemaestro")
@click.pass_context
def cli(ctx):
    """[bold]oemaestro[/bold] -- Maestro format converter for OpenEye Toolkits.

    Convert between Schrodinger Maestro files (.mae, .mae.gz, .maegz) and
    OpenEye-supported molecular formats.
    """
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())


@cli.command()
@click.argument("input_file", type=click.Path(exists=True))
@click.argument("output_file", type=click.Path())
@click.option(
    "--tags", "-t",
    multiple=True,
    type=click.Choice(list(TAG_CHOICES.keys()), case_sensitive=False),
    default=("all",),
    show_default=True,
    help="Data tag components to include. Combine multiple: -t type -t name.",
)
@click.option(
    "--perception", "-p",
    multiple=True,
    type=click.Choice(list(PERCEPTION_CHOICES.keys()), case_sensitive=False),
    default=("all",),
    show_default=True,
    help="Perception steps to run. Combine multiple: -p connectivity -p rings.",
)
@click.option(
    "--conf-test", "-c",
    type=click.Choice(CONF_TEST_CHOICES, case_sensitive=False),
    default="none",
    show_default=True,
    help="Conformer grouping test. Groups CTs sharing the same connectivity into multi-conformer molecules.",
)
@click.option(
    "--title-field",
    type=str,
    default=None,
    help="CT property to use as molecule title (e.g. 's_m_title').",
)
@click.option(
    "--count", "-n",
    type=int,
    default=None,
    help="Maximum number of molecules to convert.",
)
@click.option(
    "--append", "-a",
    is_flag=True,
    default=False,
    help="Append to output file instead of overwriting.",
)
@click.option(
    "--quiet", "-q",
    is_flag=True,
    default=False,
    help="Suppress progress output.",
)
@click.option(
    "--sd-tag-filter",
    type=str,
    multiple=True,
    help="Only keep SD data tags matching this pattern (supports * wildcards). Can be specified multiple times.",
)
def convert(input_file, output_file, tags, perception, conf_test, title_field,
            count, append, quiet, sd_tag_filter):
    """Convert a Maestro file to an OpenEye-supported format.

    Reads molecules from INPUT_FILE (Maestro format) and writes them to
    OUTPUT_FILE in the format determined by its extension.

    \b
    Supported output formats:
      .sdf          SD file
      .mol2         Tripos Mol2
      .pdb          Protein Data Bank
      .smi / .ism   SMILES / Isomeric SMILES
      .oeb / .oeb.gz  OpenEye binary
      .xyz          XYZ coordinates
      .csv          CSV
      .mol          MDL Molfile
      .can          Canonical SMILES

    \b
    Examples:
      oemaestro convert input.maegz output.sdf
      oemaestro convert input.mae output.pdb --perception none
      oemaestro convert multi.maegz mols.sdf --conf-test isomeric
      oemaestro convert input.mae output.sdf --tags name --tags type
      oemaestro convert input.mae out.sdf -n 10 --quiet
    """
    from openeye import oechem

    input_path = Path(input_file)
    output_path = Path(output_file)

    if not _is_maestro_input(input_file):
        click.secho(
            f"Error: Input file must be a Maestro file ({', '.join(sorted(MAESTRO_EXTENSIONS))}), "
            f"got '{_get_file_ext(input_file)}'",
            fg="red", err=True,
        )
        sys.exit(1)

    out_ext = _get_file_ext(output_file)
    if out_ext not in SUPPORTED_OUTPUT_EXTENSIONS:
        click.secho(
            f"Error: Unsupported output format '{out_ext}'. "
            f"Supported: {', '.join(sorted(SUPPORTED_OUTPUT_EXTENSIONS.keys()))}",
            fg="red", err=True,
        )
        sys.exit(1)

    config = OEMaestroReaderConfig()
    config.tags = _parse_tag_flags(tags)
    config.perception = _parse_perception_flags(perception)

    reader = OEMaestroReader(str(input_path), config=config)
    ct_conf_test = _build_conf_test(conf_test)

    ofs = oechem.oemolostream()
    if append:
        ofs.openappend(str(output_path))
    else:
        ofs.open(str(output_path))

    if not ofs.IsValid():
        click.secho(f"Error: Cannot open output file '{output_path}'", fg="red", err=True)
        sys.exit(1)

    import fnmatch

    def _filter_sd_tags(mol):
        if not sd_tag_filter:
            return
        to_remove = []
        for pair in oechem.OEGetSDDataPairs(mol):
            tag = pair.GetTag()
            if not any(fnmatch.fnmatch(tag, pat) for pat in sd_tag_filter):
                to_remove.append(tag)
        for tag in to_remove:
            oechem.OEDeleteSDData(mol, tag)

    mol_count = 0
    conf_count = 0

    if ct_conf_test is not None:
        pending = None
        conf_count = 0

        for ct_mol in reader:
            if title_field:
                title = oechem.OEGetSDData(ct_mol, title_field)
                if title:
                    ct_mol.SetTitle(title)

            if pending is None:
                pending = oechem.OEMol(ct_mol)
                conf_count = 1
            elif ct_conf_test.CompareMols(pending, ct_mol):
                pending.NewConf(ct_mol)
                conf_count += 1
            else:
                _filter_sd_tags(pending)
                oechem.OEWriteMolecule(ofs, pending)
                mol_count += 1
                if not quiet:
                    click.echo(
                        f"  Wrote: {pending.GetTitle() or '(untitled)'} "
                        f"({pending.NumAtoms()} atoms, {conf_count} conformer(s))",
                    )
                if count and mol_count >= count:
                    pending = None
                    break
                pending = oechem.OEMol(ct_mol)
                conf_count = 1

        if pending is not None and (not count or mol_count < count):
            _filter_sd_tags(pending)
            oechem.OEWriteMolecule(ofs, pending)
            mol_count += 1
            if not quiet:
                click.echo(
                    f"  Wrote: {pending.GetTitle() or '(untitled)'} "
                    f"({pending.NumAtoms()} atoms, {conf_count} conformer(s))",
                )
    else:
        for mol in reader:
            if title_field:
                title = oechem.OEGetSDData(mol, title_field)
                if title:
                    mol.SetTitle(title)

            _filter_sd_tags(mol)
            oechem.OEWriteMolecule(ofs, mol)
            mol_count += 1
            if not quiet:
                click.echo(
                    f"  Wrote: {mol.GetTitle() or '(untitled)'} "
                    f"({mol.NumAtoms()} atoms)",
                )
            if count and mol_count >= count:
                break

    ofs.close()

    if not quiet:
        click.secho(
            f"\nConverted {mol_count} molecule(s) -> {output_path}",
            fg="green",
        )


@cli.command()
@click.argument("input_file", type=click.Path(exists=True))
@click.option(
    "--tags", "-t",
    multiple=True,
    type=click.Choice(list(TAG_CHOICES.keys()), case_sensitive=False),
    default=("all",),
    show_default=True,
    help="Data tag components to include.",
)
@click.option(
    "--perception", "-p",
    multiple=True,
    type=click.Choice(list(PERCEPTION_CHOICES.keys()), case_sensitive=False),
    default=("all",),
    show_default=True,
    help="Perception steps to run.",
)
@click.option(
    "--count", "-n",
    type=int,
    default=None,
    help="Maximum number of molecules to inspect.",
)
@click.option(
    "--show-tags", is_flag=True, default=False,
    help="Show SD data tags for each molecule.",
)
@click.option(
    "--show-residues", is_flag=True, default=False,
    help="Show unique residues per molecule.",
)
@click.option(
    "--show-atoms", is_flag=True, default=False,
    help="Show per-atom details (name, element, coords).",
)
@click.option(
    "--atom-limit",
    type=int,
    default=20,
    show_default=True,
    help="Maximum atoms to display per molecule when --show-atoms is set.",
)
def info(input_file, tags, perception, count, show_tags, show_residues,
         show_atoms, atom_limit):
    """Display summary information about molecules in a Maestro file.

    \b
    Examples:
      oemaestro info input.mae
      oemaestro info protein.maegz --show-residues
      oemaestro info multi.mae --show-tags -n 5
      oemaestro info ligand.mae --show-atoms --atom-limit 50
    """
    from openeye import oechem

    if not _is_maestro_input(input_file):
        click.secho(
            f"Error: Input must be a Maestro file, got '{_get_file_ext(input_file)}'",
            fg="red", err=True,
        )
        sys.exit(1)

    config = OEMaestroReaderConfig()
    config.tags = _parse_tag_flags(tags)
    config.perception = _parse_perception_flags(perception)

    reader = OEMaestroReader(str(input_file), config=config)

    total_atoms = 0
    total_bonds = 0
    mol_count = 0

    for mol in reader:
        mol_count += 1
        natoms = mol.NumAtoms()
        nbonds = mol.NumBonds()
        total_atoms += natoms
        total_bonds += nbonds

        title = mol.GetTitle() or "(untitled)"
        click.secho(f"\n--- Molecule {mol_count}: {title} ---", fg="cyan", bold=True)
        click.echo(f"  Atoms: {natoms}  Bonds: {nbonds}")

        chains = set()
        residues = {}
        for atom in mol.GetAtoms():
            res = oechem.OEAtomGetResidue(atom)
            chain = res.GetChainID()
            chains.add(chain)
            rkey = (chain, res.GetResidueNumber(), res.GetName().strip())
            if rkey not in residues:
                residues[rkey] = 0
            residues[rkey] += 1

        if chains - {" ", ""}:
            click.echo(f"  Chains: {', '.join(sorted(chains - {' ', ''}))}")
            click.echo(f"  Residues: {len(residues)}")

        if show_tags:
            tag_pairs = list(oechem.OEGetSDDataPairs(mol))
            if tag_pairs:
                click.secho("  SD Data Tags:", fg="yellow")
                for pair in tag_pairs:
                    val = pair.GetValue()
                    display_val = val[:60] + "..." if len(val) > 60 else val
                    click.echo(f"    {pair.GetTag()} = {display_val}")
            else:
                click.echo("  SD Data Tags: (none)")

        if show_residues:
            click.secho("  Residues:", fg="yellow")
            for (chain, resnum, resname), atom_count in sorted(residues.items()):
                click.echo(f"    {chain}:{resname}:{resnum} ({atom_count} atoms)")

        if show_atoms:
            click.secho("  Atoms:", fg="yellow")
            displayed = 0
            for atom in mol.GetAtoms():
                if displayed >= atom_limit:
                    remaining = natoms - displayed
                    click.echo(f"    ... and {remaining} more atoms")
                    break
                res = oechem.OEAtomGetResidue(atom)
                coords = mol.GetCoords(atom)
                click.echo(
                    f"    {atom.GetName()!r:6s} Z={atom.GetAtomicNum():2d} "
                    f"({coords[0]:8.3f}, {coords[1]:8.3f}, {coords[2]:8.3f}) "
                    f"{res.GetChainID()}:{res.GetName().strip()}:{res.GetResidueNumber()}"
                )
                displayed += 1

        if count and mol_count >= count:
            break

    click.secho(f"\n{'=' * 40}", fg="green")
    click.secho(
        f"Total: {mol_count} molecule(s), {total_atoms} atoms, {total_bonds} bonds",
        fg="green", bold=True,
    )


@cli.command(name="formats")
def list_formats():
    """List supported input and output file formats."""
    click.secho("\nInput formats (Maestro):", fg="cyan", bold=True)
    click.echo("  .mae        Maestro file")
    click.echo("  .mae.gz     Maestro file (gzip compressed)")
    click.echo("  .maegz      Maestro file (gzip compressed)")

    click.secho("\nOutput formats (OpenEye):", fg="cyan", bold=True)
    for ext, desc in sorted(SUPPORTED_OUTPUT_EXTENSIONS.items()):
        click.echo(f"  {ext:12s}  {desc}")

    click.secho("\nConformer grouping tests:", fg="cyan", bold=True)
    click.echo("  none           No grouping (each CT = separate molecule)")
    click.echo("  default        OEDefaultConfTest")
    click.echo("  isomeric       OEIsomericConfTest (includes stereochemistry)")
    click.echo("  absolute       OEAbsoluteConfTest")
    click.echo("  abs-canonical  OEAbsCanonicalConfTest")

    click.secho("\nTag format components:", fg="cyan", bold=True)
    click.echo("  all    Include full Maestro key (type_owner_name)")
    click.echo("  none   Strip all data tags")
    click.echo("  name   Property name only")
    click.echo("  type   Data type prefix (s_, r_, i_, b_)")
    click.echo("  owner  Owner prefix (m_, user_, etc.)")

    click.secho("\nPerception steps:", fg="cyan", bold=True)
    click.echo("  all             Run all perception (default)")
    click.echo("  none            Skip all perception")
    click.echo("  connectivity    OEDetermineConnectivity")
    click.echo("  rings           OEFindRingAtomsAndBonds")
    click.echo("  bond-orders     OEPerceiveBondOrders")
    click.echo("  implicit-h      OEAssignImplicitHydrogens")
    click.echo("  formal-charges  OEAssignFormalCharges")
    click.echo()


def main():
    cli()


if __name__ == "__main__":
    main()
