# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
