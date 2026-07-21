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

TEST_F(MaestroWriterTest, NormalizesReversedBondEndpoints) {
    // Regression: the legacy MMCT m2io reader (structconvert, Maestro import)
    // requires i_m_from < i_m_to. A MaestroBond whose atom1_index exceeds its
    // atom2_index must still be emitted with from < to.
    auto path = (tmp_dir_ / "revbond.mae").string();
    MaestroMol mol;
    mol.title = "revbond";

    MaestroAtom a;
    a.atomic_number = 6;
    a.x = 0.0; a.y = 0.0; a.z = 0.0;
    mol.atoms.push_back(a);

    MaestroAtom b;
    b.atomic_number = 6;
    b.x = 1.5; b.y = 0.0; b.z = 0.0;
    mol.atoms.push_back(b);

    MaestroBond bond;
    bond.atom1_index = 1;  // reversed: from-atom index > to-atom index
    bond.atom2_index = 0;
    bond.order = 1;
    mol.bonds.push_back(bond);

    {
        MaestroWriter writer(path);
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

TEST_F(MaestroWriterTest, SanitizesWhitespaceInPropertyKeys) {
    // Regression: Maestro/m2io property keys are whitespace-delimited tokens.
    // A key containing a space (e.g. a POSIT SD tag "POSIT receptor filename")
    // is rejected by both the legacy MMCT reader and by maeparser, so the
    // writer must emit a whitespace-free key.
    auto path = (tmp_dir_ / "spacedkey.mae").string();
    MaestroMol mol;
    mol.title = "spacedkey";

    MaestroAtom a;
    a.atomic_number = 6;
    a.x = 0.0; a.y = 0.0; a.z = 0.0;
    mol.atoms.push_back(a);

    mol.ct_properties["s_user_POSIT receptor filename"] = "receptor.oedu";

    {
        MaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }

    // Before the fix, the emitted key contains a space and maeparser rejects
    // the block, so Read throws. After the fix the key is whitespace-free.
    MaestroReader reader(path);
    MaestroMol read_mol;
    ASSERT_TRUE(reader.Read(read_mol));

    bool found_value = false;
    for (const auto& [k, v] : read_mol.ct_properties) {
        EXPECT_EQ(k.find(' '), std::string::npos)
            << "emitted property key contains whitespace: " << k;
        if (v == "receptor.oedu") found_value = true;
    }
    EXPECT_TRUE(found_value) << "property value did not survive round-trip";
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

TEST_F(MaestroWriterTest, WritesGzipFilenameRoundTrip) {
    std::string path = (tmp_dir_ / "test.mae.gz").string();
    auto mol = make_simple_maestro_mol();
    mol.title = "gztest";
    {
        MaestroWriter writer(path);
        EXPECT_TRUE(writer.Write(mol));
        writer.Close();
    }
    // The file must be genuinely gzip-compressed (magic bytes 0x1f 0x8b).
    {
        std::ifstream raw(path, std::ios::binary);
        unsigned char magic[2] = {0, 0};
        raw.read(reinterpret_cast<char*>(magic), 2);
        EXPECT_EQ(magic[0], 0x1fu);
        EXPECT_EQ(magic[1], 0x8bu);
    }
    MaestroReader reader(path);
    MaestroMol read_mol;
    ASSERT_TRUE(reader.Read(read_mol));
    EXPECT_EQ(read_mol.title, "gztest");
    EXPECT_EQ(read_mol.NumAtoms(), mol.NumAtoms());
}

TEST_F(MaestroWriterTest, UnwritableGzipPathThrows) {
    std::string path = (tmp_dir_ / "no_such_subdir" / "x.mae.gz").string();
    EXPECT_THROW(MaestroWriter(path, WRITE_CREATE), MaestroParseError);
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
