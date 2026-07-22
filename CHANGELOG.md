# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.8.5]

### Fixed

- Atoms are now written with a per-atom display color (`s_m_color_rgb`).
  OpenEye molecules carry no Maestro color, and a structure written without one
  is rendered entirely in magenta/pink when opened in Maestro. The converter now
  assigns each atom its Maestro "Element" color-scheme color. The values are
  Maestro's own colors for every element (Z 1-118), captured by round-tripping a
  one-atom-per-element structure through Maestro, including the shared colors
  Maestro uses for grouped elements (noble gases, alkali/alkaline-earth metals,
  the superheavy block, ...); unrecognized elements fall back to a neutral gray.
  Written files now display with standard element coloring by default.

## [0.8.3]

### Fixed

- The Maestro reader no longer creates duplicate bonds. Maestro `m_bond` blocks
  can list the same bond in both directions (`a->b` and `b->a`); the converter
  previously created a parallel `OEBond` for each entry, producing molecules that
  serialized to invalid SD/MOL bond blocks OEChem itself could not read back
  (observed on prepared structures, pose-viewer files, and complexes). The
  converter now skips a bond whose atom pair is already bonded.

## [0.8.2]

### Fixed

- Bonds are now written with `i_m_from < i_m_to`. The writer previously copied
  `OEBondBase::GetBgnIdx()` / `GetEndIdx()` into `i_m_from` / `i_m_to` without
  ordering them, so any bond whose begin-atom index exceeded its end-atom index
  (common for PDB/protein-derived molecules) produced a file the legacy MMCT
  m2io reader (`structconvert`, Maestro import) rejected with
  `ct_m2io_get_bonds(): Error setting bond style`. maeparser and the oemaestro
  reader tolerated either ordering, so a pure oemaestro round-trip did not catch
  it.
- Maestro property keys are now sanitized to remove whitespace. SD-data tags
  containing spaces (e.g. OpenEye POSIT's `POSIT receptor filename`) were
  emitted verbatim as keys such as `s_user_POSIT receptor filename`; whitespace
  is invalid inside a whitespace-delimited m2io key, so both MMCT and maeparser
  aborted reading the CT. Whitespace in emitted CT- and atom-level keys is now
  replaced with underscores.

## [0.8.1]

### Fixed

- Layer-1 `MaestroReader(filename)` and `MaestroWriter(filename)` now honor
  `.mae.gz` / `.maegz` filenames, matching the Layer-3 readers/writers and the
  behavior documented in their headers. Previously the filename constructors
  passed the path straight to maeparser, which cannot open gzip in this build
  (compiled without boost::iostreams), so compressed files failed to open.
- Opening a gzip Maestro file that cannot be opened now raises
  `MaestroParseError` (as the `MaestroReader` / `MaestroWriter` constructors
  document) instead of failing silently; the gzip stream adapters throw when
  `gzopen` fails.
- Editable / source builds via the CMake presets now link OpenEye's shared
  libraries (`OPENEYE_USE_SHARED=ON`), matching the release wheels. A static
  OpenEye build loads a second OpenEye runtime with its own tag registry, so
  CT-level generic-data / SD-data tags did not interoperate with the `openeye`
  Python package and CT properties were silently dropped by `ConvertToMaestro`
  / `ConvertToOE` in both directions.

### Changed

- Consolidated the duplicated `IsGzipFilename` extension check into a single
  shared `OEMaestro::is_gzip_filename` helper in `StreamAdapter`.

## [0.8.0]

### Changed

- **Breaking:** `MolConverter::Convert` is split into two directional methods,
  `ConvertToOE` (MaestroMol → OEMolBase, the read direction) and
  `ConvertToMaestro` (OEMolBase → MaestroMol, the write direction). Both
  directions were previously overloads of a single `Convert`. The write overload
  `Convert(MaestroMol&, const OEMolBase&)` shared its signature with the
  pre-0.5.0 read argument order `Convert(maestro_mol, oe_mol)`, so through the
  Python bindings that call bound silently to the writer — returning an empty
  molecule and overwriting the source `MaestroMol` with no error raised.
  Distinct names remove the overload set entirely: an argument-order mistake is
  now a compile error in C++ and a `TypeError` in Python.

### Migration

- Read direction:
  `converter.Convert(oe_mol, maestro_mol)` → `converter.ConvertToOE(oe_mol, maestro_mol)`
- Write direction:
  `converter.Convert(maestro_mol, oe_mol)` → `converter.ConvertToMaestro(maestro_mol, oe_mol)`

The Layer-3 APIs (`OEMaestroReader` / `OEReadMaestro` for reading and
`OEMaestroWriter` / `OEWriteMaestro` for writing) are unaffected.
