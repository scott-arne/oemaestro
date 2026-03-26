#include <gtest/gtest.h>
#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/OEReadMaestro.h"
#include "oemaestro/Error.h"
#include <oechem.h>

static const std::string DATA_DIR = TEST_DATA_DIR;

TEST(OEMaestroReaderTest, ReadSingleMolecule) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/simple.mae");
    OEChem::OEMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9);
    EXPECT_STREQ(mol.GetTitle(), "Ethanol");
    ASSERT_FALSE(reader.Read(mol));  // EOF
}

TEST(OEMaestroReaderTest, ReadMultipleMolecules) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/multi.mae");
    OEChem::OEMol mol;
    int count = 0;
    while (reader.Read(mol)) count++;
    EXPECT_EQ(count, 2);
}

TEST(OEMaestroReaderTest, DefaultConfTestNoGrouping) {
    // With default conf test, conformers.mae should yield 2 separate molecules
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/conformers.mae");
    OEChem::OEMol mol;
    int count = 0;
    while (reader.Read(mol)) {
        EXPECT_EQ(mol.NumConfs(), 1);
        count++;
    }
    EXPECT_EQ(count, 2);
}

TEST(OEMaestroReaderTest, IsomericConfTestGrouping) {
    // With OEIsomericConfTest, conformers.mae should yield 1 mol with 2 confs
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/conformers.mae");
    reader.SetConfTest(new OEChem::OEIsomericConfTest());
    OEChem::OEMol mol;
    int count = 0;
    while (reader.Read(mol)) count++;
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mol.NumConfs(), 2);
}

TEST(OEMaestroReaderTest, SetPerceptionAfterConstruction) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/simple.mae");
    reader.SetPerception(OEMaestro::PERCEPTION_NONE);
    EXPECT_EQ(reader.GetPerception(), OEMaestro::PERCEPTION_NONE);
    OEChem::OEMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9);
}

TEST(OEMaestroReaderTest, SetTagFormatAfterConstruction) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/simple.mae");
    reader.SetTagFormat(OEMaestro::TAG_NAME);
    EXPECT_EQ(reader.GetTagFormat(), OEMaestro::TAG_NAME);
}

TEST(OEMaestroReaderTest, GetConfig) {
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.tags = OEMaestro::TAG_NAME;
    cfg.perception = OEMaestro::PERCEPTION_NONE;
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/simple.mae", cfg);
    auto config = reader.GetConfig();
    EXPECT_EQ(config.tags, OEMaestro::TAG_NAME);
    EXPECT_EQ(config.perception, OEMaestro::PERCEPTION_NONE);
}

TEST(OEMaestroReaderTest, GzipFile) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/simple.mae.gz");
    OEChem::OEMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9);
}

TEST(OEMaestroReaderTest, MoveConstruct) {
    OEMaestro::OEMaestroReader reader1(DATA_DIR + "/simple.mae");
    OEMaestro::OEMaestroReader reader2(std::move(reader1));
    OEChem::OEMol mol;
    ASSERT_TRUE(reader2.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9);
}

// OEReadMaestro free function tests
TEST(OEReadMaestroTest, SingleMolOverload) {
    OEChem::OEGraphMol mol;
    bool ok = OEMaestro::OEReadMaestro(DATA_DIR + "/simple.mae", mol);
    ASSERT_TRUE(ok);
    EXPECT_EQ(mol.NumAtoms(), 9);
    EXPECT_STREQ(mol.GetTitle(), "Ethanol");
}

TEST(OEReadMaestroTest, IteratorOverload) {
    auto reader = OEMaestro::OEReadMaestro(DATA_DIR + "/multi.mae");
    int count = 0;
    OEChem::OEMol mol;
    while (reader.Read(mol)) count++;
    EXPECT_EQ(count, 2);
}

TEST(OEReadMaestroTest, SingleMolEOF) {
    OEChem::OEGraphMol mol;
    // Reading from a file that doesn't exist
    bool ok = false;
    try {
        ok = OEMaestro::OEReadMaestro("/nonexistent.mae", mol);
    } catch (...) {
        ok = false;
    }
    EXPECT_FALSE(ok);
}

TEST(OEMaestroReaderTest, DefaultPerceptionIsReduced) {
    OEMaestro::OEMaestroReaderConfig cfg;
    EXPECT_EQ(cfg.perception, OEMaestro::PERCEPTION_DEFAULT);
    EXPECT_TRUE(cfg.perception & OEMaestro::PERCEPTION_RINGS);
    EXPECT_TRUE(cfg.perception & OEMaestro::PERCEPTION_IMPLICIT_HYDROGENS);
    EXPECT_TRUE(cfg.perception & OEMaestro::PERCEPTION_FORMAL_CHARGES);
    EXPECT_FALSE(cfg.perception & OEMaestro::PERCEPTION_CONNECTIVITY);
    EXPECT_FALSE(cfg.perception & OEMaestro::PERCEPTION_BOND_ORDERS);
}
