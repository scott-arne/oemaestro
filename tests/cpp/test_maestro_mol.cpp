#include <gtest/gtest.h>
#include "oemaestro/MaestroMol.h"

TEST(MaestroMolTest, DefaultValues) {
    OEMaestro::MaestroAtom atom;
    EXPECT_EQ(atom.atomic_number, 0);
    EXPECT_DOUBLE_EQ(atom.x, 0.0);
    EXPECT_DOUBLE_EQ(atom.occupancy, 1.0);
    EXPECT_EQ(atom.formal_charge, 0);
    EXPECT_TRUE(atom.atom_name.empty());
}

TEST(MaestroMolTest, NumAtomsAndBonds) {
    OEMaestro::MaestroMol mol;
    EXPECT_EQ(mol.NumAtoms(), 0);
    EXPECT_EQ(mol.NumBonds(), 0);
    mol.atoms.push_back({6, 0.0, 0.0, 0.0, 0});
    mol.atoms.push_back({8, 1.0, 0.0, 0.0, 0});
    mol.bonds.push_back({0, 1, 1});
    EXPECT_EQ(mol.NumAtoms(), 2);
    EXPECT_EQ(mol.NumBonds(), 1);
}

TEST(MaestroMolTest, ToString) {
    OEMaestro::MaestroMol mol;
    mol.title = "test";
    mol.atoms.push_back({6, 1.0, 2.0, 3.0, 0});
    auto s = mol.ToString();
    EXPECT_NE(s.find("test"), std::string::npos);
    EXPECT_NE(s.find("atoms=1"), std::string::npos);
}

#include <Reader.hpp>
#include <fstream>

static const std::string DATA_DIR = TEST_DATA_DIR;

TEST(MaestroTestData, SimpleMaeParsesWithMaeparser) {
    auto stream = std::make_shared<std::ifstream>(DATA_DIR + "/simple.mae");
    ASSERT_TRUE(stream->is_open());
    schrodinger::mae::Reader reader(stream);
    auto block = reader.next("f_m_ct");
    ASSERT_NE(block, nullptr);
}

TEST(MaestroTestData, MultiMaeParsesWithMaeparser) {
    auto stream = std::make_shared<std::ifstream>(DATA_DIR + "/multi.mae");
    ASSERT_TRUE(stream->is_open());
    schrodinger::mae::Reader reader(stream);
    ASSERT_NE(reader.next("f_m_ct"), nullptr);
    ASSERT_NE(reader.next("f_m_ct"), nullptr);
}

TEST(MaestroTestData, ProteinMaeParsesWithMaeparser) {
    auto stream = std::make_shared<std::ifstream>(DATA_DIR + "/protein.mae");
    ASSERT_TRUE(stream->is_open());
    schrodinger::mae::Reader reader(stream);
    ASSERT_NE(reader.next("f_m_ct"), nullptr);
}

TEST(MaestroTestData, ConformersMaeParsesWithMaeparser) {
    auto stream = std::make_shared<std::ifstream>(DATA_DIR + "/conformers.mae");
    ASSERT_TRUE(stream->is_open());
    schrodinger::mae::Reader reader(stream);
    ASSERT_NE(reader.next("f_m_ct"), nullptr);
    ASSERT_NE(reader.next("f_m_ct"), nullptr);
}
