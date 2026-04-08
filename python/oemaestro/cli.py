"""Command-line interface for oemaestro.

Converts Maestro format files (.mae, .mae.gz, .maegz) to OpenEye-supported
molecular formats.
"""
import sys
from pathlib import Path

import rich_click as click

from oemaestro import (
    __version__,
    OEMaestroReader,
    OEMaestroReaderConfig,
    OEMaestroWriter,
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
    ".mae": "Maestro",
    ".mae.gz": "Maestro (compressed)",
    ".maegz": "Maestro (compressed)",
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


@click.command(context_settings={"max_content_width": 125})
@click.argument("input_file", type=click.Path(exists=True))
@click.argument("output_file", type=click.Path())
@click.version_option(__version__, prog_name="oemaestro")
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
@click.option(
    "--threads",
    type=int,
    default=None,
    help="Number of threads for parallel reading (default: min(2, cpu_count)).",
)
def cli(input_file, output_file, tags, perception, conf_test, title_field,
        count, append, quiet, sd_tag_filter, threads):
    """Convert a Maestro file to another molecular format.

    Reads molecules from INPUT_FILE (.mae, .mae.gz, .maegz) and writes
    them to OUTPUT_FILE in the format determined by its extension.

    \b
    Supported output formats:
      .mae            Maestro
      .mae.gz         Maestro (compressed)
      .maegz          Maestro (compressed)
      .sdf            SD file
      .mol2           Tripos Mol2
      .pdb            Protein Data Bank
      .smi / .ism     SMILES / Isomeric SMILES
      .oeb / .oeb.gz  OpenEye binary
      .xyz            XYZ coordinates
      .csv            CSV
      .mol            MDL Molfile
      .can            Canonical SMILES

    \b
    Tag format components (--tags):
      all    Include full Maestro key (type_owner_name)
      none   Strip all data tags
      name   Property name only
      type   Data type prefix (s_, r_, i_, b_)
      owner  Owner prefix (m_, user_, etc.)

    \b
    Perception steps (--perception):
      all             Run all perception (default)
      none            Skip all perception
      connectivity    OEDetermineConnectivity
      rings           OEFindRingAtomsAndBonds
      bond-orders     OEPerceiveBondOrders
      implicit-h      OEAssignImplicitHydrogens
      formal-charges  OEAssignFormalCharges

    \b
    Conformer grouping tests (--conf-test):
      none           No grouping (each CT = separate molecule)
      default        OEDefaultConfTest
      isomeric       OEIsomericConfTest (includes stereochemistry)
      absolute       OEAbsoluteConfTest
      abs-canonical  OEAbsCanonicalConfTest

    \b
    Examples:
      oemaestro input.maegz output.sdf
      oemaestro input.mae output.pdb --perception none
      oemaestro multi.maegz mols.sdf --conf-test isomeric
      oemaestro input.mae output.sdf --tags name --tags type
      oemaestro input.mae out.sdf -n 10 --quiet
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
    config.SetTags(_parse_tag_flags(tags))
    config.SetPerception(_parse_perception_flags(perception))
    if threads is not None:
        config.SetNumThreads(threads)

    reader = OEMaestroReader(str(input_path), config=config)
    ct_conf_test = _build_conf_test(conf_test)
    if ct_conf_test is not None:
        reader.set_conf_test(ct_conf_test)

    # Determine if output is Maestro format (use OEMaestroWriter) or OpenEye format
    mae_output = out_ext in MAESTRO_EXTENSIONS

    if mae_output:
        from oemaestro import WRITE_APPEND, WRITE_CREATE
        mode = WRITE_APPEND if append else WRITE_CREATE
        writer = OEMaestroWriter(str(output_path), mode=mode)
    else:
        ofs = oechem.oemolostream()
        if append:
            ofs.openappend(str(output_path))
        else:
            ofs.open(str(output_path))

        if not ofs.IsValid():
            click.secho(f"Error: Cannot open output file '{output_path}'", fg="red", err=True)
            sys.exit(1)

    import fnmatch

    def _filter_data_tags(mol):
        if not sd_tag_filter:
            return
        to_remove = []
        for dp in mol.GetDataIter():
            tag_name = oechem.OEGetTag(dp.GetTag())
            if not any(fnmatch.fnmatch(tag_name, pat) for pat in sd_tag_filter):
                to_remove.append(tag_name)
        for tag_name in to_remove:
            mol.DeleteData(oechem.OEGetTag(tag_name))

    def _write_mol(mol):
        if mae_output:
            writer.write(mol)
        else:
            oechem.OEWriteMolecule(ofs, mol)

    from rich.console import Console

    console = Console(stderr=True)
    mol_count = 0

    def _status_text():
        label = "structure" if mol_count == 1 else "structures"
        return f"[bold cyan]{mol_count:,}[/] {label} written"

    if quiet:
        for mol in reader:
            if title_field:
                title = mol.GetStringData(title_field)
                if title:
                    mol.SetTitle(title)
            _filter_data_tags(mol)
            _write_mol(mol)
            mol_count += 1
            if count and mol_count >= count:
                break
    else:
        with console.status(_status_text(), spinner="dots", spinner_style="cyan") as status:
            for mol in reader:
                if title_field:
                    title = mol.GetStringData(title_field)
                    if title:
                        mol.SetTitle(title)
                _filter_data_tags(mol)
                _write_mol(mol)
                mol_count += 1
                status.update(_status_text())
                if count and mol_count >= count:
                    break

    if mae_output:
        writer.close()
    else:
        ofs.close()

    if not quiet:
        label = "structure" if mol_count == 1 else "structures"
        console.print(
            f"  [bold green]\u2713[/] [bold]{mol_count:,}[/] {label} written "
            f"\u2192 [magenta]{output_path}[/]"
        )


def main():
    cli()


if __name__ == "__main__":
    main()
