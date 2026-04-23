"""Tests for oemaestro oeio plugin integration."""

import pytest

oeio = pytest.importorskip("oeio", reason="oeio not installed")
oechem = pytest.importorskip("openeye.oechem", reason="OpenEye Toolkits not installed")


DATA_DIR = str(
    __import__("pathlib").Path(__file__).resolve().parent.parent / "data"
)


class TestPluginDiscovery:
    """Verify the oemaestro plugin is discovered by oeio."""

    def test_maestro_in_formats(self):
        """oeio.formats() includes 'Maestro' after oemaestro is loaded."""
        import oemaestro  # noqa: F401 - trigger handler registration

        names = [f.name for f in oeio.formats()]
        assert "Maestro" in names

    def test_maestro_format_info(self):
        """Maestro FormatInfo has correct extensions."""
        import oemaestro  # noqa: F401

        for fmt in oeio.formats():
            if fmt.name == "Maestro":
                assert ".mae" in fmt.extensions
                assert ".mae.gz" in fmt.extensions
                assert ".maegz" in fmt.extensions
                assert fmt.supports_read is True
                assert fmt.supports_write is True
                return
        pytest.fail("Maestro format not found in oeio.formats()")


class TestReadViaMaestro:
    """Test reading Maestro files through oeio."""

    def test_read_mae(self):
        """oeio.read() handles .mae files."""
        import oemaestro  # noqa: F401

        mols = list(oeio.read(f"{DATA_DIR}/simple.mae"))
        assert len(mols) == 1
        assert mols[0].NumAtoms() == 9

    def test_read_mae_gz(self):
        """oeio.read() handles .mae.gz files."""
        import oemaestro  # noqa: F401

        mols = list(oeio.read(f"{DATA_DIR}/simple.mae.gz"))
        assert len(mols) == 1
        assert mols[0].NumAtoms() == 9

    def test_read_multi(self):
        """oeio.read() reads multiple molecules from .mae."""
        import oemaestro  # noqa: F401

        mols = list(oeio.read(f"{DATA_DIR}/multi.mae"))
        assert len(mols) == 2

    def test_read_returns_oegraphmol(self):
        """Molecules returned by oeio.read are OEGraphMol instances."""
        import oemaestro  # noqa: F401

        for mol in oeio.read(f"{DATA_DIR}/simple.mae"):
            assert isinstance(mol, oechem.OEGraphMol)


class TestWriteViaMaestro:
    """Test writing Maestro files through oeio."""

    def test_write_roundtrip(self, tmp_path):
        """Write .mae via oeio, read back, verify."""
        import oemaestro  # noqa: F401

        out_path = str(tmp_path / "output.mae")
        with oeio.write(out_path) as writer:
            for mol in oeio.read(f"{DATA_DIR}/simple.mae"):
                writer.add(mol)

        mols = list(oeio.read(out_path))
        assert len(mols) == 1
        assert mols[0].NumAtoms() == 9

    def test_write_gz_roundtrip(self, tmp_path):
        """Write .mae.gz via oeio, read back, verify."""
        import oemaestro  # noqa: F401

        out_path = str(tmp_path / "output.mae.gz")
        with oeio.write(out_path) as writer:
            for mol in oeio.read(f"{DATA_DIR}/simple.mae"):
                writer.add(mol)

        mols = list(oeio.read(out_path))
        assert len(mols) == 1
