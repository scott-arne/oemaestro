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
