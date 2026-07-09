"""Unit tests for oemaestro Python bindings."""
import os
import pytest

try:
    from openeye import oechem
except ImportError:
    pytest.skip("OpenEye Toolkits not available", allow_module_level=True)


@pytest.fixture
def data_dir():
    """Path to test data directory."""
    from pathlib import Path
    return str(Path(__file__).parent.parent / "data")


class TestOEMaestroReader:
    def test_iterate_simple(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/simple.mae")
        mols = list(reader)
        assert len(mols) == 1
        assert mols[0].NumAtoms() == 9

    def test_iterate_multi(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/multi.mae")
        mols = list(reader)
        assert len(mols) == 2

    def test_gzip(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/simple.mae.gz")
        mols = list(reader)
        assert len(mols) == 1
        assert mols[0].NumAtoms() == 9

    def test_residue_info(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/protein.mae")
        mol = next(iter(reader))
        found_ala = False
        for atom in mol.GetAtoms():
            res = oechem.OEAtomGetResidue(atom)
            if res.GetName().strip() == "ALA":
                found_ala = True
                assert res.GetChainID() == "A"
        assert found_ala

    def test_read_method(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/simple.mae")
        mol = oechem.OEGraphMol()
        ok = reader.read(mol)
        assert ok
        assert mol.NumAtoms() == 9
        ok = reader.read(mol)
        assert not ok

    def test_repr(self, data_dir):
        from oemaestro import OEMaestroReader
        reader = OEMaestroReader(f"{data_dir}/simple.mae")
        r = repr(reader)
        assert "OEMaestroReader" in r
        assert "tags=" in r

    def test_with_config(self, data_dir):
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig, TAG_NAME, PERCEPTION_NONE
        config = OEMaestroReaderConfig()
        config.SetTags(TAG_NAME)
        config.SetPerception(PERCEPTION_NONE)
        reader = OEMaestroReader(f"{data_dir}/simple.mae", config=config)
        mol = next(iter(reader))
        assert mol.NumAtoms() == 9


class TestOEReadMaestro:
    def test_single_mol(self, data_dir):
        from oemaestro import OEReadMaestro
        mol = oechem.OEGraphMol()
        ok = OEReadMaestro(f"{data_dir}/simple.mae", mol)
        assert ok
        assert mol.NumAtoms() == 9

    def test_iterator_mode(self, data_dir):
        from oemaestro import OEReadMaestro
        reader = OEReadMaestro(f"{data_dir}/multi.mae")
        mols = list(reader)
        assert len(mols) == 2

    def test_single_mol_with_config(self, data_dir):
        from oemaestro import OEReadMaestro, OEMaestroReaderConfig, PERCEPTION_NONE
        config = OEMaestroReaderConfig()
        config.SetPerception(PERCEPTION_NONE)
        mol = oechem.OEGraphMol()
        ok = OEReadMaestro(f"{data_dir}/simple.mae", mol, config=config)
        assert ok
        assert mol.NumAtoms() == 9


class TestTagFormatting:
    def test_tag_format_via_config(self, data_dir):
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig, TAG_NAME
        cfg = OEMaestroReaderConfig()
        cfg.SetTags(TAG_NAME)
        reader = OEMaestroReader(f"{data_dir}/protein.mae", config=cfg)
        assert reader.get_tag_format() == TAG_NAME

    def test_tag_none_no_tags(self, data_dir):
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig, TAG_NONE
        cfg = OEMaestroReaderConfig()
        cfg.SetTags(TAG_NONE)
        reader = OEMaestroReader(f"{data_dir}/protein.mae", config=cfg)
        mol = next(iter(reader))
        tags = list(mol.GetDataIter())
        assert len(tags) == 0


class TestPerception:
    def test_perception_via_config(self, data_dir):
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig, PERCEPTION_NONE
        cfg = OEMaestroReaderConfig()
        cfg.SetPerception(PERCEPTION_NONE)
        reader = OEMaestroReader(f"{data_dir}/simple.mae", config=cfg)
        assert reader.get_perception() == PERCEPTION_NONE

    def test_perception_none(self, data_dir):
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig, PERCEPTION_NONE
        cfg = OEMaestroReaderConfig()
        cfg.SetPerception(PERCEPTION_NONE)
        reader = OEMaestroReader(f"{data_dir}/simple.mae", config=cfg)
        mol = next(iter(reader))
        assert mol.NumAtoms() > 0


class TestSWIGTypemaps:
    def test_no_serialization(self, data_dir):
        """Verify OpenEye molecules pass through without serialization."""
        from oemaestro import OEReadMaestro
        mol = oechem.OEGraphMol()
        ok = OEReadMaestro(f"{data_dir}/simple.mae", mol)
        assert ok
        assert isinstance(mol, oechem.OEGraphMol)
        assert mol.NumAtoms() > 0

    def test_oemol_type(self, data_dir):
        """Verify OEMol works through OEMolBase typemap."""
        from oemaestro import OEReadMaestro
        mol = oechem.OEMol()
        ok = OEReadMaestro(f"{data_dir}/simple.mae", mol)
        assert ok
        assert mol.NumAtoms() > 0


class TestEnums:
    def test_tag_bitmask(self):
        from oemaestro import TAG_TYPE, TAG_OWNER, TAG_NAME, TAG_ALL, TAG_NONE
        assert TAG_ALL == (TAG_TYPE | TAG_OWNER | TAG_NAME)
        assert TAG_NONE == 0

    def test_perception_bitmask(self):
        from oemaestro import (PERCEPTION_CONNECTIVITY, PERCEPTION_RINGS,
                                PERCEPTION_BOND_ORDERS, PERCEPTION_ALL, PERCEPTION_NONE)
        assert PERCEPTION_NONE == 0
        assert (PERCEPTION_ALL & PERCEPTION_CONNECTIVITY) != 0

    def test_all_enums_importable(self):
        from oemaestro import (
            TAG_NONE, TAG_TYPE, TAG_OWNER, TAG_NAME, TAG_ALL,
            PERCEPTION_NONE, PERCEPTION_CONNECTIVITY, PERCEPTION_RINGS,
            PERCEPTION_BOND_ORDERS, PERCEPTION_IMPLICIT_HYDROGENS,
            PERCEPTION_FORMAL_CHARGES, PERCEPTION_ALL,
        )


class TestLowLevelAPI:
    def test_maestro_reader(self, data_dir):
        from oemaestro import MaestroReader, MaestroMol
        reader = MaestroReader(f"{data_dir}/simple.mae")
        mol = MaestroMol()
        ok = reader.Read(mol)
        assert ok
        assert mol.NumAtoms() == 9
        assert mol.NumBonds() == 8

    def test_mol_converter(self, data_dir):
        from oemaestro import MaestroReader, MaestroMol, MolConverter
        reader = MaestroReader(f"{data_dir}/simple.mae")
        mmol = MaestroMol()
        reader.Read(mmol)
        converter = MolConverter()
        mol = oechem.OEGraphMol()
        converter.ConvertToOE(mol, mmol)
        assert mol.NumAtoms() == 9

    def test_mol_converter_direction_is_unambiguous(self, data_dir):
        """The two conversion directions have distinct names so an argument-order
        mistake is a loud error rather than a silent wrong-direction conversion.

        Regression test for the pre-0.5.0 ``Convert(maestro_mol, oe_mol)`` order
        that used to bind silently to the write overload and return an empty
        molecule.
        """
        from oemaestro import MaestroReader, MaestroMol, MolConverter

        reader = MaestroReader(f"{data_dir}/simple.mae")
        mmol = MaestroMol()
        reader.Read(mmol)
        converter = MolConverter()

        # The overloaded ``Convert`` no longer exists.
        assert not hasattr(converter, "Convert")

        # Read direction populates the OpenEye molecule.
        mol = oechem.OEGraphMol()
        converter.ConvertToOE(mol, mmol)
        assert mol.NumAtoms() == 9

        # Write direction round-trips back to the IR.
        back = MaestroMol()
        converter.ConvertToMaestro(back, mol)
        assert back.NumAtoms() == 9

        # Passing the arguments in the wrong order is a loud TypeError, not a
        # silent empty result.
        with pytest.raises(TypeError):
            converter.ConvertToOE(mmol, mol)

    def test_maestro_mol_repr(self, data_dir):
        from oemaestro import MaestroReader, MaestroMol
        reader = MaestroReader(f"{data_dir}/simple.mae")
        mmol = MaestroMol()
        reader.Read(mmol)
        r = repr(mmol)
        assert "Ethanol" in r


# ---------------------------------------------------------------------------
# 8G66 PDB vs Maestro comparison tests
# ---------------------------------------------------------------------------

def _atom_key(mol, atom):
    """Build a unique atom key from residue info and atom name."""
    res = oechem.OEAtomGetResidue(atom)
    return (
        res.GetChainID(),
        res.GetResidueNumber(),
        res.GetName().strip(),
        atom.GetName(),
    )


@pytest.fixture(scope="module")
def pdb_mol():
    """Read 8G66 from PDB format as reference."""
    from pathlib import Path
    data = str(Path(__file__).parent.parent / "data")
    mol = oechem.OEGraphMol()
    ifs = oechem.oemolistream(f"{data}/8G66.pdb")
    oechem.OEReadMolecule(ifs, mol)
    ifs.close()
    return mol


@pytest.fixture(scope="module")
def mae_mol():
    """Read 8G66 from Maestro (.maegz) format."""
    from pathlib import Path
    from oemaestro import OEMaestroReader
    data = str(Path(__file__).parent.parent / "data")
    return next(iter(OEMaestroReader(f"{data}/8G66.maegz")))


@pytest.fixture(scope="module")
def pdb_atom_lookup(pdb_mol):
    """Map (chain, resnum, resname_stripped, atomname) -> atom for PDB mol."""
    lookup = {}
    for atom in pdb_mol.GetAtoms():
        key = _atom_key(pdb_mol, atom)
        lookup[key] = atom
    return lookup


@pytest.fixture(scope="module")
def mae_atom_lookup(mae_mol):
    """Map (chain, resnum, resname_stripped, atomname) -> atom for MAE mol."""
    lookup = {}
    for atom in mae_mol.GetAtoms():
        key = _atom_key(mae_mol, atom)
        lookup[key] = atom
    return lookup


class TestMaeVsPdbAtomCounts:
    def test_same_atom_count(self, pdb_mol, mae_mol):
        assert pdb_mol.NumAtoms() == mae_mol.NumAtoms()

    def test_same_bond_count(self, pdb_mol, mae_mol):
        assert pdb_mol.NumBonds() == mae_mol.NumBonds()

    def test_atom_key_sets_identical(self, pdb_atom_lookup, mae_atom_lookup):
        """Every atom identified by (chain, resnum, resname, name) in PDB
        must also exist in MAE, and vice versa."""
        pdb_keys = set(pdb_atom_lookup.keys())
        mae_keys = set(mae_atom_lookup.keys())
        assert pdb_keys == mae_keys, (
            f"PDB-only: {pdb_keys - mae_keys}, MAE-only: {mae_keys - pdb_keys}"
        )


class TestMaeVsPdbAtomNames:
    def test_exact_atom_names_with_spaces(self, pdb_atom_lookup, mae_atom_lookup):
        """Atom names must match exactly including PDB-style whitespace padding."""
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_name = pdb_atom_lookup[key].GetName()
            mae_name = mae_atom_lookup[key].GetName()
            if pdb_name != mae_name:
                mismatches.append((key, pdb_name, mae_name))
        assert mismatches == [], (
            f"{len(mismatches)} atom name mismatches, first 5: {mismatches[:5]}"
        )

    def test_atom_name_length_is_four(self, mae_atom_lookup):
        """Maestro atom names should be 4-character PDB-style names."""
        bad = []
        for key, atom in mae_atom_lookup.items():
            name = atom.GetName()
            if len(name) != 4:
                bad.append((key, name, len(name)))
        assert bad == [], f"{len(bad)} atoms with non-4-char names: {bad[:5]}"


class TestMaeVsPdbElements:
    def test_atomic_numbers_match(self, pdb_atom_lookup, mae_atom_lookup):
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_z = pdb_atom_lookup[key].GetAtomicNum()
            mae_z = mae_atom_lookup[key].GetAtomicNum()
            if pdb_z != mae_z:
                mismatches.append((key, pdb_z, mae_z))
        assert mismatches == [], (
            f"{len(mismatches)} element mismatches: {mismatches[:5]}"
        )


class TestMaeVsPdbCoordinates:
    def test_3d_coordinates_exact(self, pdb_mol, mae_mol, pdb_atom_lookup, mae_atom_lookup):
        """3D coordinates must match to within floating-point tolerance (< 0.001 A)."""
        max_diff = 0.0
        worst_key = None
        failures = []
        for key in pdb_atom_lookup:
            pa = pdb_atom_lookup[key]
            ma = mae_atom_lookup[key]
            pc = pdb_mol.GetCoords(pa)
            mc = mae_mol.GetCoords(ma)
            dx = abs(pc[0] - mc[0])
            dy = abs(pc[1] - mc[1])
            dz = abs(pc[2] - mc[2])
            d = max(dx, dy, dz)
            if d > max_diff:
                max_diff = d
                worst_key = key
            if d > 0.001:
                failures.append((key, pc, mc, d))
        assert failures == [], (
            f"{len(failures)} coords differ > 0.001 A, worst: {failures[0]}"
        )

    def test_coordinates_are_nonzero(self, mae_mol, mae_atom_lookup):
        """Sanity check that coordinates are populated, not all zeros."""
        nonzero = 0
        for key, atom in mae_atom_lookup.items():
            coords = mae_mol.GetCoords(atom)
            if any(abs(c) > 0.01 for c in coords):
                nonzero += 1
        assert nonzero > 0.99 * len(mae_atom_lookup)


class TestMaeVsPdbResidueInfo:
    def test_residue_names_match_stripped(self, pdb_atom_lookup, mae_atom_lookup):
        """Residue names must match when stripped of whitespace."""
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_res = oechem.OEAtomGetResidue(pdb_atom_lookup[key])
            mae_res = oechem.OEAtomGetResidue(mae_atom_lookup[key])
            if pdb_res.GetName().strip() != mae_res.GetName().strip():
                mismatches.append((key, pdb_res.GetName(), mae_res.GetName()))
        assert mismatches == [], f"{len(mismatches)} residue name mismatches: {mismatches[:5]}"

    def test_residue_numbers_match(self, pdb_atom_lookup, mae_atom_lookup):
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_res = oechem.OEAtomGetResidue(pdb_atom_lookup[key])
            mae_res = oechem.OEAtomGetResidue(mae_atom_lookup[key])
            if pdb_res.GetResidueNumber() != mae_res.GetResidueNumber():
                mismatches.append((key, pdb_res.GetResidueNumber(), mae_res.GetResidueNumber()))
        assert mismatches == [], f"{len(mismatches)} resnum mismatches: {mismatches[:5]}"

    def test_chain_ids_match(self, pdb_atom_lookup, mae_atom_lookup):
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_res = oechem.OEAtomGetResidue(pdb_atom_lookup[key])
            mae_res = oechem.OEAtomGetResidue(mae_atom_lookup[key])
            if pdb_res.GetChainID() != mae_res.GetChainID():
                mismatches.append((key, pdb_res.GetChainID(), mae_res.GetChainID()))
        assert mismatches == [], f"{len(mismatches)} chain mismatches: {mismatches[:5]}"

    def test_insertion_codes_match(self, pdb_atom_lookup, mae_atom_lookup):
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_res = oechem.OEAtomGetResidue(pdb_atom_lookup[key])
            mae_res = oechem.OEAtomGetResidue(mae_atom_lookup[key])
            if pdb_res.GetInsertCode() != mae_res.GetInsertCode():
                mismatches.append((key, pdb_res.GetInsertCode(), mae_res.GetInsertCode()))
        assert mismatches == [], f"{len(mismatches)} insert code mismatches: {mismatches[:5]}"

    def test_unique_chains(self, pdb_mol, mae_mol):
        """Both molecules should have the same set of chain IDs."""
        pdb_chains = {oechem.OEAtomGetResidue(a).GetChainID() for a in pdb_mol.GetAtoms()}
        mae_chains = {oechem.OEAtomGetResidue(a).GetChainID() for a in mae_mol.GetAtoms()}
        assert pdb_chains == mae_chains

    def test_unique_residues(self, pdb_mol, mae_mol):
        """Both molecules should have the same set of unique residues."""
        def get_residues(mol):
            residues = set()
            for atom in mol.GetAtoms():
                res = oechem.OEAtomGetResidue(atom)
                residues.add((res.GetChainID(), res.GetResidueNumber(), res.GetName().strip()))
            return residues
        assert get_residues(pdb_mol) == get_residues(mae_mol)


class TestMaeVsPdbBFactorsAndOccupancy:
    def test_bfactors_match(self, pdb_atom_lookup, mae_atom_lookup):
        """B-factors must match to within 0.01."""
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_bf = oechem.OEAtomGetResidue(pdb_atom_lookup[key]).GetBFactor()
            mae_bf = oechem.OEAtomGetResidue(mae_atom_lookup[key]).GetBFactor()
            if abs(pdb_bf - mae_bf) > 0.01:
                mismatches.append((key, pdb_bf, mae_bf))
        assert mismatches == [], f"{len(mismatches)} bfactor mismatches: {mismatches[:5]}"

    def test_occupancies_match(self, pdb_atom_lookup, mae_atom_lookup):
        """Occupancy values must match to within 0.01."""
        mismatches = []
        for key in pdb_atom_lookup:
            pdb_occ = oechem.OEAtomGetResidue(pdb_atom_lookup[key]).GetOccupancy()
            mae_occ = oechem.OEAtomGetResidue(mae_atom_lookup[key]).GetOccupancy()
            if abs(pdb_occ - mae_occ) > 0.01:
                mismatches.append((key, pdb_occ, mae_occ))
        assert mismatches == [], f"{len(mismatches)} occupancy mismatches: {mismatches[:5]}"


class TestMaeVsPdbBondConnectivity:
    def test_bond_connectivity_identical(self, pdb_mol, mae_mol):
        """Bond connectivity (ignoring bond order) must be identical.

        Bond orders for symmetric carboxylates (ASP OD1/OD2, GLU OE1/OE2) may
        differ between PDB and Maestro due to resonance assignment, so we only
        check connectivity here.
        """
        def get_connectivity(mol):
            bonds = set()
            for bond in mol.GetBonds():
                k1 = _atom_key(mol, bond.GetBgn())
                k2 = _atom_key(mol, bond.GetEnd())
                bonds.add((min(k1, k2), max(k1, k2)))
            return bonds

        pdb_conn = get_connectivity(pdb_mol)
        mae_conn = get_connectivity(mae_mol)
        pdb_only = pdb_conn - mae_conn
        mae_only = mae_conn - pdb_conn
        assert pdb_only == set(), f"Bonds in PDB not MAE: {list(pdb_only)[:5]}"
        assert mae_only == set(), f"Bonds in MAE not PDB: {list(mae_only)[:5]}"

    def test_bond_orders_match_except_resonance(self, pdb_mol, mae_mol):
        """Bond orders should match except for resonance-equivalent carboxylates.

        ASP (CG-OD1/OD2) and GLU (CD-OE1/OE2) have interchangeable single/double
        bond assignments. We count those separately and verify all other bond
        orders match exactly.
        """
        RESONANCE_ATOMS = {
            "ASP": {" OD1", " OD2"},
            "GLU": {" OE1", " OE2"},
            "ARG": {" NH1", " NH2"},
        }

        def is_resonance_bond(mol, bond):
            a1, a2 = bond.GetBgn(), bond.GetEnd()
            r1 = oechem.OEAtomGetResidue(a1)
            resname = r1.GetName().strip()
            if resname in RESONANCE_ATOMS:
                names = {a1.GetName(), a2.GetName()}
                if names & RESONANCE_ATOMS[resname]:
                    return True
            return False

        def get_bonds_with_order(mol):
            bonds = {}
            for bond in mol.GetBonds():
                k1 = _atom_key(mol, bond.GetBgn())
                k2 = _atom_key(mol, bond.GetEnd())
                key = (min(k1, k2), max(k1, k2))
                bonds[key] = (bond.GetOrder(), is_resonance_bond(mol, bond))
            return bonds

        pdb_bonds = get_bonds_with_order(pdb_mol)
        mae_bonds = get_bonds_with_order(mae_mol)

        non_resonance_mismatches = []
        resonance_mismatches = 0
        for key in pdb_bonds:
            if key not in mae_bonds:
                continue
            pdb_order, pdb_res = pdb_bonds[key]
            mae_order, mae_res = mae_bonds[key]
            if pdb_order != mae_order:
                if pdb_res or mae_res:
                    resonance_mismatches += 1
                else:
                    non_resonance_mismatches.append((key, pdb_order, mae_order))

        assert non_resonance_mismatches == [], (
            f"{len(non_resonance_mismatches)} non-resonance bond order mismatches: "
            f"{non_resonance_mismatches[:5]}"
        )


class TestThreadedReading:
    def test_threaded_reader_config(self):
        """Verify num_threads passes through to C++ reader."""
        from oemaestro import OEMaestroReaderConfig
        cfg = OEMaestroReaderConfig()
        assert cfg.GetNumThreads() >= 1
        cfg.SetNumThreads(4)
        assert cfg.GetNumThreads() == 4

    def test_threaded_reader_reads_molecules(self, data_dir):
        """Verify threaded reader produces same results as single-threaded."""
        from oemaestro import OEMaestroReader, OEMaestroReaderConfig

        cfg1 = OEMaestroReaderConfig()
        cfg1.SetNumThreads(1)
        titles1 = [mol.GetTitle() for mol in OEMaestroReader(f"{data_dir}/multi.mae", cfg1)]

        cfg4 = OEMaestroReaderConfig()
        cfg4.SetNumThreads(4)
        titles4 = [mol.GetTitle() for mol in OEMaestroReader(f"{data_dir}/multi.mae", cfg4)]

        assert titles1 == titles4

    def test_perception_default(self):
        """Verify PERCEPTION_DEFAULT is accessible from Python."""
        from oemaestro import PERCEPTION_DEFAULT, PERCEPTION_ALL
        assert PERCEPTION_DEFAULT != PERCEPTION_ALL


class TestCLI:
    def test_help(self):
        """Verify CLI --help runs without import errors."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-m', 'oemaestro.cli', '--help'],
            capture_output=True, text=True, timeout=30,
        )
        assert result.returncode == 0
        assert 'Usage' in result.stdout

    def test_version(self):
        """Verify CLI --version returns the package version."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-m', 'oemaestro.cli', '--version'],
            capture_output=True, text=True, timeout=30,
        )
        assert result.returncode == 0
        from oemaestro import __version__
        assert __version__ in result.stdout

    def test_convert_to_sdf(self, data_dir):
        """Verify CLI converts a Maestro file to SDF."""
        import subprocess
        import sys
        import tempfile
        with tempfile.NamedTemporaryFile(suffix='.sdf', delete=False) as f:
            out_path = f.name
        try:
            result = subprocess.run(
                [sys.executable, '-m', 'oemaestro.cli',
                 f'{data_dir}/simple.mae', out_path, '--quiet'],
                capture_output=True, text=True, timeout=30,
            )
            assert result.returncode == 0
            assert os.path.getsize(out_path) > 0
        finally:
            os.unlink(out_path)

    def test_convert_to_mae(self, data_dir):
        """Verify CLI converts a Maestro file to Maestro."""
        import subprocess
        import sys
        import tempfile
        from oemaestro import OEMaestroReader
        with tempfile.NamedTemporaryFile(suffix='.mae', delete=False) as f:
            out_path = f.name
        try:
            result = subprocess.run(
                [sys.executable, '-m', 'oemaestro.cli',
                 f'{data_dir}/multi.mae', out_path, '--quiet'],
                capture_output=True, text=True, timeout=30,
            )
            assert result.returncode == 0
            mols = list(OEMaestroReader(out_path))
            assert len(mols) == 2
        finally:
            os.unlink(out_path)

    def test_convert_to_maegz(self, data_dir):
        """Verify CLI converts a Maestro file to compressed Maestro."""
        import subprocess
        import sys
        import tempfile
        from oemaestro import OEMaestroReader
        with tempfile.NamedTemporaryFile(suffix='.maegz', delete=False) as f:
            out_path = f.name
        try:
            result = subprocess.run(
                [sys.executable, '-m', 'oemaestro.cli',
                 f'{data_dir}/simple.mae', out_path, '--quiet'],
                capture_output=True, text=True, timeout=30,
            )
            assert result.returncode == 0
            mols = list(OEMaestroReader(out_path))
            assert len(mols) == 1
        finally:
            os.unlink(out_path)
