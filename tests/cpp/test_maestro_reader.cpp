#include <gtest/gtest.h>
#include <fstream>
#include "oemaestro/MaestroReader.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"

static const std::string DATA_DIR = TEST_DATA_DIR;

TEST(MaestroReaderTest, ReadSimpleMolecule) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/simple.mae");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.title, "Ethanol");
    EXPECT_EQ(mol.NumAtoms(), 9u);
    EXPECT_EQ(mol.NumBonds(), 8u);
    // Verify first atom is carbon (Z=6)
    EXPECT_EQ(mol.atoms[0].atomic_number, 6);
    EXPECT_NEAR(mol.atoms[0].x, 1.2, 0.001);
    EXPECT_NEAR(mol.atoms[0].y, 0.0, 0.001);
    EXPECT_NEAR(mol.atoms[0].z, 0.0, 0.001);
    // Verify bond indices are 0-based
    EXPECT_EQ(mol.bonds[0].atom1_index, 0);
    EXPECT_EQ(mol.bonds[0].atom2_index, 1);
    EXPECT_EQ(mol.bonds[0].order, 1);
}

TEST(MaestroReaderTest, ReadMultipleCTs) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/multi.mae");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));   // First molecule (Ethanol)
    EXPECT_EQ(mol.title, "Ethanol");
    EXPECT_EQ(mol.NumAtoms(), 9u);
    ASSERT_TRUE(reader.Read(mol));   // Second molecule (Methane)
    EXPECT_EQ(mol.title, "Methane");
    EXPECT_EQ(mol.NumAtoms(), 5u);
    EXPECT_EQ(mol.NumBonds(), 4u);
    ASSERT_FALSE(reader.Read(mol));  // End of file
}

TEST(MaestroReaderTest, ReadProteinWithResidueInfo) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/protein.mae");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.title, "Ala-Gly Dipeptide");
    EXPECT_EQ(mol.NumAtoms(), 13u);
    EXPECT_EQ(mol.NumBonds(), 12u);
    bool found_ala = false;
    for (const auto& atom : mol.atoms) {
        if (atom.residue_name.find("ALA") != std::string::npos) {
            found_ala = true;
            EXPECT_EQ(atom.chain_id, "A");
            EXPECT_GT(atom.residue_number, 0);
            break;
        }
    }
    EXPECT_TRUE(found_ala);
    // Verify structural props NOT in properties map
    for (const auto& atom : mol.atoms) {
        EXPECT_EQ(atom.properties.count("i_m_atomic_number"), 0u);
        EXPECT_EQ(atom.properties.count("r_m_x_coord"), 0u);
        EXPECT_EQ(atom.properties.count("s_m_pdb_atom_name"), 0u);
    }
}

TEST(MaestroReaderTest, ReadsGzipFilename) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/simple.mae.gz");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.title, "Ethanol");
    EXPECT_EQ(mol.NumAtoms(), 9u);
}

TEST(MaestroReaderTest, NonExistentFileThrows) {
    EXPECT_THROW(
        OEMaestro::MaestroReader("/nonexistent/file.mae"),
        OEMaestro::MaestroParseError
    );
}

TEST(MaestroReaderTest, EndOfFileReturnsFalse) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/simple.mae");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    ASSERT_FALSE(reader.Read(mol));
}

TEST(MaestroReaderTest, StreamConstructor) {
    auto* fs = new std::ifstream(DATA_DIR + "/simple.mae", std::ios::binary);
    ASSERT_TRUE(fs->is_open());
    std::shared_ptr<std::istream> stream(fs);
    OEMaestro::MaestroReader reader(stream);
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.title, "Ethanol");
    EXPECT_EQ(mol.NumAtoms(), 9u);
}

TEST(MaestroReaderTest, ProteinBfactorAndOccupancy) {
    OEMaestro::MaestroReader reader(DATA_DIR + "/protein.mae");
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader.Read(mol));
    // First atom (N of ALA) should have bfactor=20.0 and occupancy=1.0
    EXPECT_NEAR(mol.atoms[0].bfactor, 20.0, 0.01);
    EXPECT_NEAR(mol.atoms[0].occupancy, 1.0, 0.01);
}

TEST(MaestroReaderTest, MoveConstructor) {
    OEMaestro::MaestroReader reader1(DATA_DIR + "/simple.mae");
    OEMaestro::MaestroReader reader2(std::move(reader1));
    OEMaestro::MaestroMol mol;
    ASSERT_TRUE(reader2.Read(mol));
    EXPECT_EQ(mol.title, "Ethanol");
}

TEST(StreamAdapterTest, IsGzipFilename) {
    EXPECT_TRUE(OEMaestro::is_gzip_filename("x.mae.gz"));
    EXPECT_TRUE(OEMaestro::is_gzip_filename("x.maegz"));
    EXPECT_FALSE(OEMaestro::is_gzip_filename("x.mae"));
    EXPECT_FALSE(OEMaestro::is_gzip_filename("x.gz"));
    EXPECT_FALSE(OEMaestro::is_gzip_filename(""));
}
