# oemaestro

Native Maestro format (.mae, .mae.gz, .maegz) parser for
[OpenEye Toolkits](https://docs.eyesopen.com/toolkits/python/index.html).
Reads Schrodinger Maestro files directly into OpenEye molecule objects, with
support for multi-threaded reading, conformer grouping, and design unit
extraction.

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Python API](#python-api)
- [Command-Line Interface](#command-line-interface)
- [Configuration](#configuration)
- [License](#license)

## Installation

**Requirements:**

- Python 3.10 or later
- OpenEye Toolkits 2025.2 or later (with a valid license)

Install from a built wheel (Linux / OSX):

```bash
pip install oemaestro
```

### Building from Source

Building from source requires CMake 3.21+, SWIG 4.0+, and the OpenEye C++ Toolkits. You can build
everything including the Python package using:

```bash
python scripts/build_python.py            \
    --openeye-root /path/to/openeye/cpp/  \
    --python /path/to/python3
```

## Quick Start

```python
from oemaestro import OEMaestroReader

for mol in OEMaestroReader("input.mae"):
    print(mol.GetTitle(), mol.NumAtoms())
```

## Python API

### Reading Molecules

You can read molecules using several different patterns:

```python
from openeye import oechem
from oemaestro import OEMaestroReader

# Iterate over all molecules in a file given a path
for mol in OEMaestroReader("structures.maegz"):
    ...

# Iterate over all molecules in a file given a stream
ifs = oechem.oeifstream("structures.maegz")
for mol in OEMaestroReader(ifs):
    ...
ifs.close()

# Iterate over all molecules in a file reusing a molecule object
mol = oechem.OEGraphMol()
while OEMaestroReader("structures.maegz", mol):
    ...

# Or by using a stream
ifs = oechem.oeifstream("structures.maegz")
while OEMaestroReader(ifs, mol):
    ...
ifs.close()
```

### Multi-Threaded Reading

Speed up reading of large files by distributing parsing and conversion across
multiple threads:

```python
from oemaestro import OEMaestroReader, OEMaestroReaderConfig

config = OEMaestroReaderConfig()
config.SetNumThreads(4)

for mol in OEMaestroReader("large_library.maegz", config=config):
    process(mol)
```

### Conformer Grouping

Group consecutive CT blocks that share the same connectivity into
multi-conformer molecules:

```python
from openeye import oechem
from oemaestro import OEMaestroReader

reader = OEMaestroReader("poses.mae")
reader.set_conf_test(oechem.OEIsomericConfTest())

for mol in reader:
    print(f"{mol.GetTitle()}: {mol.NumConfs()} conformer(s)")
```

Available OpenEye conformer tests: `OEDefaultConfTest`, `OEIsomericConfTest`,
`OEAbsoluteConfTest`, `OEAbsCanonicalConfTest`.

### Design Units

Read design units using the same exact API as ```OEReadMaestro```. For example:

```python
from openeye import oechem
from oemaestro import OEMaestroDesignUnitReader

for du in OEMaestroDesignUnitReader("prepared.mae"):
    protein = oechem.OEGraphMol()
    du.GetProtein(protein)
    print(f"Protein: {protein.NumAtoms()} atoms")
```

## Command-Line Interface

The `oemaestro` command converts Maestro files to any OpenEye-supported
molecular format.

```
oemaestro INPUT_FILE OUTPUT_FILE [OPTIONS]
```

**Basic usage:**

```bash
oemaestro input.mae output.sdf
oemaestro structures.maegz molecules.pdb --perception none
oemaestro poses.mae grouped.sdf --conf-test isomeric --threads 4
```

**Supported output formats:** SDF, MOL2, PDB, SMILES, OEB, XYZ, CSV, MDL
Molfile, Canonical SMILES.

Run `oemaestro --help` for a full list of options.

## Configuration

### Tag Formatting

Maestro property keys follow a `type_owner_name` convention (e.g.,
`r_psp_IC50`). Control which components appear as SD data tags:

```python
from oemaestro import OEMaestroReaderConfig, TAG_NAME

config = OEMaestroReaderConfig()
config.SetTags(TAG_NAME)  # Keep only the property name (e.g., "IC50")
```

Options: `TAG_ALL` (default), `TAG_NAME`, `TAG_TYPE`, `TAG_OWNER`, `TAG_NONE`.
Combine with `|` for multiple components.

### Perception

Control which chemical perception steps run after parsing:

```python
from oemaestro import (
    OEMaestroReaderConfig,
    PERCEPTION_RINGS,
    PERCEPTION_FORMAL_CHARGES,
)

config = OEMaestroReaderConfig()
config.SetPerception(PERCEPTION_RINGS | PERCEPTION_FORMAL_CHARGES)
```

Available steps: `PERCEPTION_CONNECTIVITY`, `PERCEPTION_RINGS`,
`PERCEPTION_BOND_ORDERS`, `PERCEPTION_IMPLICIT_HYDROGENS`,
`PERCEPTION_FORMAL_CHARGES`. Use `PERCEPTION_ALL` to run everything,
`PERCEPTION_DEFAULT` for the recommended subset, or `PERCEPTION_NONE` to skip
perception entirely.

## License

MIT License. See [LICENSE](LICENSE) for details.
