#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "oemaestro/MaestroWriter.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MaestroMol.h"
#include "oemaestro/Error.h"

using namespace OEMaestro;
namespace fs = std::filesystem;

namespace {

MaestroMol make_simple_maestro_mol() {
    MaestroMol mol;
    mol.title = "test_mol";

    MaestroAtom c;
    c.atomic_number = 6;
    c.x = 0.0; c.y = 0.0; c.z = 0.0;
    mol.atoms.push_back(c);

    MaestroAtom o;
    o.atomic_number = 8;
    o.x = 1.2; o.y = 0.0; o.z = 0.0;
    o.formal_charge = -1;
    mol.atoms.push_back(o);

    MaestroBond bond;
    bond.atom1_index = 0;
    bond.atom2_index = 1;
    bond.order = 2;
    mol.bonds.push_back(bond);

    mol.ct_properties["r_m_mol_weight"] = "28.01";
    return mol;
}

class MaestroWriterTest : public ::testing::Test {
protected:
    fs::path tmp_dir_;
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "oemaestro_writer_test";
        fs::create_directories(tmp_dir_);
    }
    void TearDown() override {
        fs::remove_all(tmp_dir_);
    }
};

}  // namespace

TEST_F(MaestroWriterTest, WriteAndReadBack) {
    auto path = (tmp_dir_ / "test.mae").string();
    auto mol = make_simple_maestro_mol();

    {
        MaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "test_mol");
    EXPECT_EQ(read_mol.NumAtoms(), 2u);
    EXPECT_EQ(read_mol.NumBonds(), 1u);
    EXPECT_EQ(read_mol.atoms[0].atomic_number, 6);
    EXPECT_EQ(read_mol.atoms[1].atomic_number, 8);
    EXPECT_EQ(read_mol.atoms[1].formal_charge, -1);
    EXPECT_EQ(read_mol.bonds[0].order, 2);
}

TEST_F(MaestroWriterTest, WriteMultiple) {
    auto path = (tmp_dir_ / "multi.mae").string();
    auto mol1 = make_simple_maestro_mol();
    mol1.title = "mol1";
    auto mol2 = make_simple_maestro_mol();
    mol2.title = "mol2";

    {
        MaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol1));
        EXPECT_TRUE(writer.Write(mol2));
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "mol1");
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "mol2");
    EXPECT_FALSE(reader.Read(read_mol));
}

TEST_F(MaestroWriterTest, AppendMode) {
    auto path = (tmp_dir_ / "append.mae").string();
    auto mol1 = make_simple_maestro_mol();
    mol1.title = "first";

    {
        MaestroWriter writer(path, WRITE_CREATE);
        writer.Write(mol1);
        writer.Close();
    }

    auto mol2 = make_simple_maestro_mol();
    mol2.title = "second";
    {
        MaestroWriter writer(path, WRITE_APPEND);
        writer.Write(mol2);
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "first");
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "second");
}

TEST_F(MaestroWriterTest, GzipCompression) {
    auto path = (tmp_dir_ / "test.mae.gz").string();
    auto mol = make_simple_maestro_mol();

    {
        MaestroWriter writer(path);
        writer.Write(mol);
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "test_mol");
    EXPECT_EQ(read_mol.NumAtoms(), 2u);
}

TEST_F(MaestroWriterTest, WriteToClosedThrows) {
    auto path = (tmp_dir_ / "closed.mae").string();
    MaestroWriter writer(path);
    writer.Close();

    auto mol = make_simple_maestro_mol();
    EXPECT_THROW(writer.Write(mol), OEMaestroError);
}

TEST_F(MaestroWriterTest, EmptyMolecule) {
    auto path = (tmp_dir_ / "empty.mae").string();
    MaestroMol mol;
    mol.title = "empty";

    {
        MaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    MaestroReader reader(path);
    MaestroMol read_mol;
    EXPECT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "empty");
    EXPECT_EQ(read_mol.NumAtoms(), 0u);
}

TEST_F(MaestroWriterTest, AppendGzipThrows) {
    auto path = (tmp_dir_ / "test.mae.gz").string();
    EXPECT_THROW(MaestroWriter(path, WRITE_APPEND), OEMaestroError);
}
