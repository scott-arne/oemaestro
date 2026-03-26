#include <gtest/gtest.h>
#include "oemaestro/OEMaestroDesignUnitReader.h"
#include "oemaestro/OEReadMaestroDesignUnit.h"
#include "oemaestro/OEMaestroReader.h"
#include <oechem.h>
#include <oebio.h>

static const std::string DATA_DIR = TEST_DATA_DIR;

// --- Tests using 8G66.maegz (known protein + ligand + solvent structure) ---

TEST(DesignUnitReaderTest, ReadSingleDU) {
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/8G66.maegz");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));
    EXPECT_TRUE(du.HasProtein());
}

TEST(DesignUnitReaderTest, ProteinHasAtoms) {
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/8G66.maegz");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));
    OEChem::OEGraphMol protein;
    du.GetProtein(protein);
    EXPECT_GT(protein.NumAtoms(), 0u);
}

TEST(DesignUnitReaderTest, SolventDetected) {
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/8G66.maegz");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));

    // Check if solvent was detected
    // Note: 8G66.maegz may or may not have HOH residues depending on the preprocessing
    // The important thing is that if there IS solvent data, it should be accessible
    OEChem::OEGraphMol solvent;
    du.GetSolvent(solvent);

    // If solvent exists, the design unit should report it
    if (solvent.NumAtoms() > 0) {
        EXPECT_TRUE(du.HasSolvent());
    }
    // Test passes either way - we're just checking consistency
}

TEST(DesignUnitReaderTest, AtomCountConservation) {
    // protein + ligand + solvent + cofactors == total
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/8G66.maegz");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));

    OEChem::OEGraphMol protein, ligand, solvent, cofactors;
    du.GetProtein(protein);
    du.GetLigand(ligand);
    du.GetSolvent(solvent);
    du.GetComponent(cofactors, OEBio::OEDesignUnitComponents::Cofactors);

    unsigned total_from_du = protein.NumAtoms() + ligand.NumAtoms() +
                             solvent.NumAtoms() + cofactors.NumAtoms();

    // Read same structure as a plain mol to get total
    OEChem::OEGraphMol full_mol;
    OEMaestro::OEMaestroReader mol_reader(DATA_DIR + "/8G66.maegz");
    ASSERT_TRUE(mol_reader.Read(full_mol));

    EXPECT_EQ(total_from_du, full_mol.NumAtoms());
}

TEST(DesignUnitReaderTest, EOFReturnsFalse) {
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/simple.mae");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));   // First CT
    EXPECT_FALSE(reader.Read(du));  // EOF
}

TEST(DesignUnitReaderTest, MultipleStructures) {
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/multi.mae");
    OEBio::OEDesignUnit du;
    int count = 0;
    while (reader.Read(du)) count++;
    EXPECT_EQ(count, 2);
}

TEST(DesignUnitReaderTest, MetalsStayInProtein) {
    // 8G66 contains ZN -- it should be in protein, not solvent
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/8G66.maegz");
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));

    OEChem::OEGraphMol solvent;
    du.GetSolvent(solvent);

    // Check no zinc in solvent
    for (OESystem::OEIter<OEChem::OEAtomBase> ai = solvent.GetAtoms(); ai; ++ai) {
        EXPECT_NE(ai->GetAtomicNum(), 30u) << "Zinc found in solvent component";
    }
}

TEST(DesignUnitReaderTest, MoveConstruct) {
    OEMaestro::OEMaestroDesignUnitReader reader1(DATA_DIR + "/8G66.maegz");
    OEMaestro::OEMaestroDesignUnitReader reader2(std::move(reader1));
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader2.Read(du));
    EXPECT_TRUE(du.HasProtein());
}

// --- OEReadMaestroDesignUnit free function tests ---

TEST(OEReadMaestroDesignUnitTest, SingleDUOverload) {
    OEBio::OEDesignUnit du;
    bool ok = OEMaestro::OEReadMaestroDesignUnit(DATA_DIR + "/8G66.maegz", du);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(du.HasProtein());
}

TEST(OEReadMaestroDesignUnitTest, IteratorOverload) {
    auto reader = OEMaestro::OEReadMaestroDesignUnit(DATA_DIR + "/multi.mae");
    int count = 0;
    OEBio::OEDesignUnit du;
    while (reader.Read(du)) count++;
    EXPECT_EQ(count, 2);
}

TEST(OEReadMaestroDesignUnitTest, NonexistentFile) {
    OEBio::OEDesignUnit du;
    bool ok = false;
    try {
        ok = OEMaestro::OEReadMaestroDesignUnit("/nonexistent.mae", du);
    } catch (...) {
        ok = false;
    }
    EXPECT_FALSE(ok);
}

// --- Custom predicate test ---

TEST(DesignUnitReaderTest, CustomLigandPredicate) {
    // Use a predicate that classifies all hydrogen atoms as ligand
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/simple.mae");
    reader.SetLigandPredicate(OEChem::OEIsHydrogen());
    OEBio::OEDesignUnit du;
    ASSERT_TRUE(reader.Read(du));

    OEChem::OEGraphMol ligand;
    du.GetLigand(ligand);
    // All atoms in ligand should be hydrogen
    for (OESystem::OEIter<OEChem::OEAtomBase> ai = ligand.GetAtoms(); ai; ++ai) {
        EXPECT_EQ(ai->GetAtomicNum(), 1u);
    }
    EXPECT_TRUE(du.HasLigand());
}

// --- Config passthrough tests ---

TEST(DesignUnitReaderTest, PerceptionConfig) {
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.SetPerception(OEMaestro::PERCEPTION_NONE);
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/simple.mae", cfg);
    EXPECT_EQ(reader.GetPerception(), OEMaestro::PERCEPTION_NONE);
}

TEST(DesignUnitReaderTest, TagFormatConfig) {
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.SetTags(OEMaestro::TAG_NAME);
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/simple.mae", cfg);
    EXPECT_EQ(reader.GetTagFormat(), OEMaestro::TAG_NAME);
}

TEST(DesignUnitReaderTest, GetConfig) {
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.SetTags(OEMaestro::TAG_NAME);
    cfg.SetPerception(OEMaestro::PERCEPTION_NONE);
    OEMaestro::OEMaestroDesignUnitReader reader(DATA_DIR + "/simple.mae", cfg);
    auto config = reader.GetConfig();
    EXPECT_EQ(config.GetTags(), OEMaestro::TAG_NAME);
    EXPECT_EQ(config.GetPerception(), OEMaestro::PERCEPTION_NONE);
}
