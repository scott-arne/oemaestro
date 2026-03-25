#include <gtest/gtest.h>
#include "oemaestro/ResidueClassifier.h"

TEST(ResidueClassifierTest, WaterIsSolvent) {
    EXPECT_TRUE(OEMaestro::IsSolventResidue("HOH"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("DOD"));
}

TEST(ResidueClassifierTest, OrganicSolventsAreSolvent) {
    EXPECT_TRUE(OEMaestro::IsSolventResidue("GOL"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("DMS"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("EDO"));
}

TEST(ResidueClassifierTest, BuffersAreSolvent) {
    EXPECT_TRUE(OEMaestro::IsSolventResidue("TRS"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("MES"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("EPE"));
}

TEST(ResidueClassifierTest, DetergentsAreSolvent) {
    EXPECT_TRUE(OEMaestro::IsSolventResidue("BME"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue("DTT"));
}

TEST(ResidueClassifierTest, MetalCationsAreNotSolvent) {
    EXPECT_FALSE(OEMaestro::IsSolventResidue("ZN"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("MG"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("CA"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("FE"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("NA"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("K"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("MN"));
}

TEST(ResidueClassifierTest, AnionsAreNotSolvent) {
    EXPECT_FALSE(OEMaestro::IsSolventResidue("CL"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("SO4"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("PO4"));
}

TEST(ResidueClassifierTest, NucleotideCofactors) {
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("NAD"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("FAD"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("ATP"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("ADP"));
}

TEST(ResidueClassifierTest, HemeCofactors) {
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("HEM"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("HEC"));
}

TEST(ResidueClassifierTest, CoenzymeCofactors) {
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("COA"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("SAM"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("PLP"));
}

TEST(ResidueClassifierTest, StandardAminoAcidsNotClassified) {
    EXPECT_FALSE(OEMaestro::IsSolventResidue("ALA"));
    EXPECT_FALSE(OEMaestro::IsCofactorResidue("ALA"));
    EXPECT_FALSE(OEMaestro::IsSolventResidue("GLY"));
    EXPECT_FALSE(OEMaestro::IsCofactorResidue("GLY"));
}

TEST(ResidueClassifierTest, WhitespaceStripping) {
    EXPECT_TRUE(OEMaestro::IsSolventResidue("HOH "));
    EXPECT_TRUE(OEMaestro::IsSolventResidue(" HOH"));
    EXPECT_TRUE(OEMaestro::IsSolventResidue(" HOH "));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("HEM "));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue(" HEM"));
}

TEST(ResidueClassifierTest, CITIsCofactorOnly) {
    EXPECT_FALSE(OEMaestro::IsSolventResidue("CIT"));
    EXPECT_TRUE(OEMaestro::IsCofactorResidue("CIT"));
}

TEST(ResidueClassifierTest, GetSolventCodesNonEmpty) {
    const auto& codes = OEMaestro::GetSolventCodes();
    EXPECT_GT(codes.size(), 40u);
    EXPECT_TRUE(codes.count("HOH"));
    EXPECT_FALSE(codes.count("ZN"));
}

TEST(ResidueClassifierTest, GetCofactorCodesNonEmpty) {
    const auto& codes = OEMaestro::GetCofactorCodes();
    EXPECT_GT(codes.size(), 50u);
    EXPECT_TRUE(codes.count("HEM"));
}
