# oemaestro

Native Maestro format (.mae, .mae.gz, .maegz) reader and writer for
[OpenEye Toolkits](https://docs.eyesopen.com/toolkits/python/index.html).
Reads and writes Schrodinger Maestro files directly with OpenEye molecule
objects, with support for multi-threaded reading, conformer grouping, design
unit extraction, and round-trip property preservation.

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Python API](#python-api)
  - [Reading Molecules](#reading-molecules)
  - [Writing Molecules](#writing-molecules)
  - [Multi-Threaded Reading](#multi-threaded-reading)
  - [Conformer Grouping](#conformer-grouping)
  - [Design Units](#design-units)
- [Command-Line Interface](#command-line-interface)
- [Configuration](#configuration)
- [License](#license)

## Installation

**Requirements:**

- Python 3.11 or later
- OpenEye Toolkits 2026.1 or later (with a valid license)

Install from a built wheel (Linux / OSX):

```bash
pip install oemaestro
```

### Building from Source

Building from source requires CMake 3.21+, SWIG 4.4.1 or newer, and the OpenEye C++ Toolkits.
CI builds with SWIG 4.5.1 (`pip install swig==4.5.1`). SWIG 4.4.0, which Ubuntu 26.04 ships as
`/usr/bin/swig`, generates stable-ABI wrappers that do not compile against Python 3.13 or newer.

On Linux, install `patchelf` as well, so editable builds can add the OpenEye library directories
to the extension's RPATH; without it the editable import fails with
`cannot open shared object file`.

You can build everything including the Python package using:

```bash
python scripts/build_python.py            \
    --openeye-root /path/to/openeye/cpp/  \
    --python /path/to/python3
```

The tracked CMake presets carry no machine paths. Set `OPENEYE_ROOT` in the
environment, or create a `CMakeUserPresets.json` (gitignored) that inherits a
tracked preset:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "local",
            "inherits": "debug",
            "cacheVariables": {
                "OPENEYE_ROOT": "/path/to/openeye/toolkits",
                "Python3_EXECUTABLE": "/path/to/python"
            }
        }
    ]
}
```

## Quick Start

```python
from oemaestro import OEMaestroReader, OEMaestroWriter

# Read molecules from a Maestro file
for mol in OEMaestroReader("input.mae"):
    print(mol.GetTitle(), mol.NumAtoms())

# Write molecules to a Maestro file
with OEMaestroWriter("output.mae") as writer:
    for mol in OEMaestroReader("input.mae"):
        writer.write(mol)
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

### Writing Molecules

Write OpenEye molecules to Maestro format files:

```python
from openeye import oechem
from oemaestro import OEMaestroWriter, OEWriteMaestro

# Write multiple molecules using a context manager
with OEMaestroWriter("output.mae") as writer:
    for mol in molecules:
        writer.write(mol)

# Write a single molecule with one call
OEWriteMaestro("output.mae", mol)

# Write gzipped Maestro files
with OEMaestroWriter("output.mae.gz") as writer:
    writer.write(mol)
```

Multi-conformer `OEMol` objects are written as one CT block per conformer.
SD data properties and typed generic data are preserved as Maestro CT-level
properties.

Configure writing with `OEMaestroWriterConfig`:

```python
from oemaestro import OEMaestroWriter, OEMaestroWriterConfig, TAG_NAME, WRITE_APPEND

config = OEMaestroWriterConfig()
config.SetTags(TAG_NAME)         # Tag format for property keys
config.SetMode(WRITE_APPEND)     # Append to an existing file
config.SetDefaultOwner("user")   # Default owner prefix for new keys

with OEMaestroWriter("output.mae", config=config) as writer:
    writer.write(mol)
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
