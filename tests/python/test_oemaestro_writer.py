import tempfile
import pytest
from pathlib import Path

try:
    from openeye import oechem
except ImportError:
    pytest.skip("OpenEye Toolkits not available", allow_module_level=True)

import oemaestro


@pytest.fixture
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield Path(d)


def _make_simple_mol():
    mol = oechem.OEGraphMol()
    c = mol.NewAtom(6)
    o = mol.NewAtom(8)
    mol.NewBond(c, o, 2)
    coords = oechem.OEFloatArray([0.0, 0.0, 0.0, 1.2, 0.0, 0.0])
    mol.SetCoords(coords)
    mol.SetTitle("test_mol")
    return mol


class TestOEMaestroWriter:
    def test_write_and_read_back(self, tmp_dir):
        mol = _make_simple_mol()
        path = tmp_dir / "test.mae"

        with oemaestro.OEMaestroWriter(str(path)) as writer:
            assert writer.write(mol)

        reader = oemaestro.OEMaestroReader(str(path))
        read_mol = next(iter(reader))
        assert read_mol.GetTitle() == "test_mol"
        assert read_mol.NumAtoms() == 2
        assert read_mol.NumBonds() == 1

    def test_context_manager(self, tmp_dir):
        mol = _make_simple_mol()
        path = tmp_dir / "ctx.mae"

        with oemaestro.OEMaestroWriter(path) as writer:
            writer.write(mol)
        # File should be closed and readable
        reader = oemaestro.OEMaestroReader(str(path))
        assert next(iter(reader)).NumAtoms() == 2

    def test_pathlib_support(self, tmp_dir):
        mol = _make_simple_mol()
        path = tmp_dir / "pathlib.mae"
        with oemaestro.OEMaestroWriter(path) as writer:
            writer.write(mol)

    def test_gzip(self, tmp_dir):
        mol = _make_simple_mol()
        path = tmp_dir / "test.mae.gz"

        with oemaestro.OEMaestroWriter(str(path)) as writer:
            writer.write(mol)

        reader = oemaestro.OEMaestroReader(str(path))
        read_mol = next(iter(reader))
        assert read_mol.GetTitle() == "test_mol"

    def test_multi_mol(self, tmp_dir):
        mol1 = _make_simple_mol()
        mol1.SetTitle("mol1")
        mol2 = _make_simple_mol()
        mol2.SetTitle("mol2")

        path = tmp_dir / "multi.mae"
        with oemaestro.OEMaestroWriter(str(path)) as writer:
            writer.write(mol1)
            writer.write(mol2)

        reader = oemaestro.OEMaestroReader(str(path))
        mols = list(reader)
        assert len(mols) == 2
        assert mols[0].GetTitle() == "mol1"
        assert mols[1].GetTitle() == "mol2"

    def test_reversed_bond_order_normalized(self, tmp_dir):
        """Regression: a bond built with begin-idx > end-idx must emit i_m_from < i_m_to.

        The legacy MMCT m2io reader (structconvert / Maestro import) rejects
        files where i_m_from > i_m_to, even though maeparser tolerates either
        ordering.
        """
        from oemaestro import MaestroReader, MaestroMol

        mol = oechem.OEGraphMol()
        a = mol.NewAtom(6)  # atom index 0
        b = mol.NewAtom(6)  # atom index 1
        mol.SetCoords(a, (0.0, 0.0, 0.0))
        mol.SetCoords(b, (1.5, 0.0, 0.0))
        mol.NewBond(b, a, 1)  # begin=idx1, end=idx0 -> reversed order
        mol.SetTitle("revbond")

        path = tmp_dir / "revbond.mae"
        with oemaestro.OEMaestroWriter(str(path)) as writer:
            assert writer.write(mol)

        # MaestroReader copies the emitted i_m_from/i_m_to into
        # atom1_index/atom2_index, so this asserts on the emitted ordering.
        reader = MaestroReader(str(path))
        mmol = MaestroMol()
        assert reader.Read(mmol)
        assert mmol.NumBonds() == 1
        bond = mmol.bonds[0]
        assert bond.atom1_index < bond.atom2_index

    def test_sd_data_key_with_space_is_readable(self, tmp_dir):
        """Regression: SD-data tags with spaces must produce readable files.

        POSIT emits tags like ``POSIT receptor filename`` (with a space). A
        Maestro property key cannot contain whitespace, so both the legacy MMCT
        reader and maeparser reject such a file. The writer must sanitize the
        key so the file round-trips.
        """
        from oemaestro import MaestroReader, MaestroMol

        mol = oechem.OEGraphMol()
        c = mol.NewAtom(6)
        mol.SetCoords(c, (0.0, 0.0, 0.0))
        mol.SetTitle("posit")
        oechem.OESetSDData(mol, "POSIT receptor filename", "receptor.oedu")

        path = tmp_dir / "posit.mae"
        with oemaestro.OEMaestroWriter(str(path)) as writer:
            assert writer.write(mol)

        # Before the fix, maeparser rejects the spaced key and Read raises.
        reader = MaestroReader(str(path))
        mmol = MaestroMol()
        assert reader.Read(mmol)
        for key in mmol.ct_properties.keys():
            assert " " not in key, f"emitted key contains whitespace: {key!r}"

    def test_layer1_writer_gzip_round_trip(self, tmp_dir):
        from oemaestro import MaestroReader, MaestroWriter, MaestroMol
        path = tmp_dir / "layer1.mae.gz"
        mmol = MaestroMol()
        # Populate via the writer converter path: read a known file, write gz, read back.
        src = MaestroReader("tests/data/simple.mae")
        assert src.Read(mmol)
        writer = MaestroWriter(str(path))
        assert writer.Write(mmol)
        writer.Close()
        # The file must be genuinely gzip-compressed (magic bytes 0x1f 0x8b).
        assert path.read_bytes()[:2] == b"\x1f\x8b"
        back = MaestroMol()
        assert MaestroReader(str(path)).Read(back)
        assert back.NumAtoms() == mmol.NumAtoms()


class TestOEWriteMaestro:
    def test_free_function(self, tmp_dir):
        mol = _make_simple_mol()
        path = tmp_dir / "free_fn.mae"
        assert oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        assert oemaestro.OEReadMaestro(str(path), read_mol)
        assert read_mol.GetTitle() == "test_mol"

    def test_round_trip(self, tmp_dir):
        """Read fixture, write, read back, compare."""
        input_path = "tests/data/simple.mae"
        reader = oemaestro.OEMaestroReader(input_path)
        mol = next(iter(reader))
        original_title = mol.GetTitle()
        original_natoms = mol.NumAtoms()

        output_path = tmp_dir / "roundtrip.mae"
        oemaestro.OEWriteMaestro(str(output_path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(output_path), read_mol)
        assert read_mol.GetTitle() == original_title
        assert read_mol.NumAtoms() == original_natoms


class TestRoundTripFixtures:
    """Round-trip integration tests reading all fixture files, writing, and reading back."""

    def test_round_trip_simple(self, tmp_dir):
        reader = oemaestro.OEMaestroReader("tests/data/simple.mae")
        mol = next(iter(reader))
        path = tmp_dir / "rt_simple.mae"
        oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(path), read_mol)
        assert read_mol.GetTitle() == mol.GetTitle()
        assert read_mol.NumAtoms() == mol.NumAtoms()
        assert read_mol.NumBonds() == mol.NumBonds()

    def test_round_trip_simple_gz(self, tmp_dir):
        reader = oemaestro.OEMaestroReader("tests/data/simple.mae.gz")
        mol = next(iter(reader))
        path = tmp_dir / "rt_simple.mae.gz"
        oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(path), read_mol)
        assert read_mol.GetTitle() == mol.GetTitle()
        assert read_mol.NumAtoms() == mol.NumAtoms()

    def test_round_trip_multi(self, tmp_dir):
        input_mols = list(oemaestro.OEMaestroReader("tests/data/multi.mae"))
        assert len(input_mols) > 1

        path = tmp_dir / "rt_multi.mae"
        with oemaestro.OEMaestroWriter(str(path)) as writer:
            for mol in input_mols:
                writer.write(mol)

        output_mols = list(oemaestro.OEMaestroReader(str(path)))
        assert len(output_mols) == len(input_mols)
        for orig, written in zip(input_mols, output_mols):
            assert written.GetTitle() == orig.GetTitle()
            assert written.NumAtoms() == orig.NumAtoms()
            assert written.NumBonds() == orig.NumBonds()

    def test_round_trip_protein(self, tmp_dir):
        reader = oemaestro.OEMaestroReader("tests/data/protein.mae")
        mol = next(iter(reader))
        path = tmp_dir / "rt_protein.mae"
        oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(path), read_mol)
        assert read_mol.NumAtoms() == mol.NumAtoms()
        assert read_mol.NumBonds() == mol.NumBonds()

        # Verify residue info survives round-trip
        orig_atoms = list(mol.GetAtoms())
        read_atoms = list(read_mol.GetAtoms())
        for oa, ra in zip(orig_atoms[:10], read_atoms[:10]):
            orig_res = oechem.OEAtomGetResidue(oa)
            read_res = oechem.OEAtomGetResidue(ra)
            assert read_res.GetName().strip() == orig_res.GetName().strip()
            assert read_res.GetResidueNumber() == orig_res.GetResidueNumber()
            assert read_res.GetChainID() == orig_res.GetChainID()

    def test_round_trip_maegz(self, tmp_dir):
        reader = oemaestro.OEMaestroReader("tests/data/8G66.maegz")
        mol = next(iter(reader))
        path = tmp_dir / "rt_8G66.mae"
        oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(path), read_mol)
        assert read_mol.NumAtoms() == mol.NumAtoms()
        assert read_mol.NumBonds() == mol.NumBonds()

    def test_round_trip_coordinates(self, tmp_dir):
        """Verify 3D coordinates survive round-trip."""
        reader = oemaestro.OEMaestroReader("tests/data/simple.mae")
        mol = next(iter(reader))

        # Collect original coords
        orig_coords = {}
        for atom in mol.GetAtoms():
            xyz = oechem.OEFloatArray(3)
            mol.GetCoords(atom, xyz)
            orig_coords[atom.GetIdx()] = (xyz[0], xyz[1], xyz[2])

        path = tmp_dir / "rt_coords.mae"
        oemaestro.OEWriteMaestro(str(path), mol)

        read_mol = oechem.OEGraphMol()
        oemaestro.OEReadMaestro(str(path), read_mol)

        for atom in read_mol.GetAtoms():
            xyz = oechem.OEFloatArray(3)
            read_mol.GetCoords(atom, xyz)
            ox, oy, oz = orig_coords[atom.GetIdx()]
            assert abs(xyz[0] - ox) < 0.01
            assert abs(xyz[1] - oy) < 0.01
            assert abs(xyz[2] - oz) < 0.01
