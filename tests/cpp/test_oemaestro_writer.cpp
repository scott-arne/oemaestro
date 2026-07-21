#include <gtest/gtest.h>
#include <filesystem>
#include <oechem.h>
#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MaestroMol.h"


using namespace OEMaestro;
namespace fs = std::filesystem;

class OEMaestroWriterTest : public ::testing::Test {
protected:
    fs::path tmp_dir_;
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "oemaestro_oewriter_test";
        fs::create_directories(tmp_dir_);
    }
    void TearDown() override {
        fs::remove_all(tmp_dir_);
    }
};

TEST_F(OEMaestroWriterTest, WriteGraphMol) {
    OEChem::OEGraphMol mol;
    auto* c = mol.NewAtom(6);
    auto* o = mol.NewAtom(8);
    mol.NewBond(c, o, 2);
    float coords[] = {0.0f, 0.0f, 0.0f, 1.2f, 0.0f, 0.0f};
    mol.SetCoords(coords);
    mol.SetTitle("C=O");

    auto path = (tmp_dir_ / "graphmol.mae").string();
    {
        OEMaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    OEMaestroReader reader(path);
    OEChem::OEGraphMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(std::string(read_mol.GetTitle()), "C=O");
    EXPECT_EQ(read_mol.NumAtoms(), 2u);
    EXPECT_EQ(read_mol.NumBonds(), 1u);
}

TEST_F(OEMaestroWriterTest, NormalizesReversedBondEndpoints) {
    // Regression: a bond whose begin-atom index exceeds its end-atom index
    // (common for PDB/protein-derived molecules) must still be written with
    // i_m_from < i_m_to, or the legacy MMCT m2io reader (structconvert,
    // Maestro import) rejects the file.
    OEChem::OEGraphMol mol;
    auto* a = mol.NewAtom(6);   // atom index 0
    auto* b = mol.NewAtom(6);   // atom index 1
    mol.NewBond(b, a, 1);       // begin=idx1, end=idx0 -> reversed order
    float coords[] = {0.0f, 0.0f, 0.0f, 1.5f, 0.0f, 0.0f};
    mol.SetCoords(coords);
    mol.SetTitle("revbond");

    auto path = (tmp_dir_ / "revbond.mae").string();
    {
        OEMaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    // MaestroReader copies the emitted i_m_from / i_m_to directly into
    // atom1_index / atom2_index, so this asserts on the emitted ordering.
    MaestroReader reader(path);
    MaestroMol read_mol;
    ASSERT_TRUE(reader.Read(read_mol));
    ASSERT_EQ(read_mol.NumBonds(), 1u);
    EXPECT_LT(read_mol.bonds[0].atom1_index, read_mol.bonds[0].atom2_index);
}

TEST_F(OEMaestroWriterTest, SanitizesWhitespaceInSDDataKeys) {
    // Regression: SD-data tags with spaces (e.g. POSIT's "POSIT receptor
    // filename") become Maestro property keys. Whitespace is invalid in a
    // Maestro key, so the emitted file must be readable back.
    OEChem::OEGraphMol mol;
    auto* c = mol.NewAtom(6);
    float coords[] = {0.0f, 0.0f, 0.0f};
    mol.SetCoords(coords);
    (void)c;
    mol.SetTitle("posit");
    OEChem::OESetSDData(mol, "POSIT receptor filename", "receptor.oedu");

    auto path = (tmp_dir_ / "posit.mae").string();
    {
        OEMaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    // Before the fix maeparser rejects the spaced key and Read throws.
    MaestroReader reader(path);
    MaestroMol read_mol;
    ASSERT_TRUE(reader.Read(read_mol));
    for (const auto& [k, v] : read_mol.ct_properties) {
        EXPECT_EQ(k.find(' '), std::string::npos)
            << "emitted property key contains whitespace: " << k;
    }
}

TEST_F(OEMaestroWriterTest, WriteMultiConformer) {
    OEChem::OEMol mol;
    auto* c = mol.NewAtom(6);
    auto* o = mol.NewAtom(8);
    mol.NewBond(c, o, 2);
    mol.SetTitle("conformers");

    float coords1[] = {0.0f, 0.0f, 0.0f, 1.2f, 0.0f, 0.0f};
    mol.SetCoords(coords1);
    float coords2[] = {0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 0.0f};
    mol.NewConf(coords2);

    auto path = (tmp_dir_ / "multiconf.mae").string();
    {
        OEMaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol mmol;
    int ct_count = 0;
    while (reader.Read(mmol)) ct_count++;
    EXPECT_EQ(ct_count, 2);
}

TEST_F(OEMaestroWriterTest, GzipWrite) {
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);
    mol.SetTitle("gzip_test");

    auto path = (tmp_dir_ / "test.mae.gz").string();
    {
        OEMaestroWriter writer(path);
        writer.Write(mol);
        writer.Close();
    }

    OEMaestroReader reader(path);
    OEChem::OEGraphMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(std::string(read_mol.GetTitle()), "gzip_test");
}

TEST_F(OEMaestroWriterTest, WithConfig) {
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);
    mol.SetTitle("config_test");
    OEChem::OESetSDData(mol, "r_m_mol_weight", "12.01");

    OEMaestroWriterConfig config;
    config.SetTags(TAG_ALL);

    auto path = (tmp_dir_ / "config.mae").string();
    {
        OEMaestroWriter writer(path, config);
        writer.Write(mol);
        writer.Close();
    }

    OEMaestroReader reader(path);
    OEChem::OEGraphMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(std::string(read_mol.GetTitle()), "config_test");
}
