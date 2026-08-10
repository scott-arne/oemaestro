#include <gtest/gtest.h>

#include <string>

#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/ReadStatus.h"
#include "oemaestro/Error.h"

static const std::string DATA_DIR = TEST_DATA_DIR;

TEST(ReadStatus, DefaultResultIsEndOfStreamAndFalsey) {
    const OEMaestro::ReadResult result;
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(OEMaestro::ReadStatus::EndOfStream, result.status);
}

TEST(ReadStatus, MaestroDeclaresItselfUnableToResynchronize) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/multi.mae");
    EXPECT_FALSE(reader.CanResynchronize());
}

TEST(ReadStatus, CleanFileReportsOkThenEndOfStream) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/multi.mae");
    OEChem::OEMol mol;
    EXPECT_EQ(OEMaestro::ReadStatus::Ok, reader.TryRead(mol).status);
    EXPECT_EQ(OEMaestro::ReadStatus::Ok, reader.TryRead(mol).status);
    EXPECT_EQ(OEMaestro::ReadStatus::EndOfStream, reader.TryRead(mol).status);
}

TEST(ReadStatus, CorruptBlockIsReportedAsARecordErrorNotAsEndOfStream) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/corrupt_second_ct.mae");

    OEChem::OEMol mol;
    const OEMaestro::ReadResult first = reader.TryRead(mol);
    EXPECT_EQ(OEMaestro::ReadStatus::Ok, first.status);
    EXPECT_STREQ("Ethanol", mol.GetTitle());

    const OEMaestro::ReadResult second = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::RecordError, second.status);
    EXPECT_FALSE(second.message.empty());

    // The honest part: the caller is told the stream cannot be continued, so a
    // truncated import is reported as truncated rather than as a clean partial.
    EXPECT_FALSE(second.resynchronized);
}

TEST(ReadStatus, TryReadDoesNotThrowWhereReadWould) {
    OEMaestro::OEMaestroReader throwing(DATA_DIR + "/corrupt_second_ct.mae");
    OEChem::OEMol mol;
    ASSERT_TRUE(throwing.Read(mol));
    EXPECT_THROW(throwing.Read(mol), OEMaestro::MaestroParseError);

    OEMaestro::OEMaestroReader quiet(DATA_DIR + "/corrupt_second_ct.mae");
    ASSERT_EQ(OEMaestro::ReadStatus::Ok, quiet.TryRead(mol).status);
    EXPECT_NO_THROW((void)quiet.TryRead(mol));
}

TEST(ReadStatus, MolBaseOverloadBehavesTheSame) {
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/corrupt_second_ct.mae");
    OEChem::OEGraphMol mol;
    EXPECT_EQ(OEMaestro::ReadStatus::Ok, reader.TryRead(mol).status);
    EXPECT_EQ(OEMaestro::ReadStatus::RecordError, reader.TryRead(mol).status);
}

TEST(ReadStatus, AfterRecordErrorStreamBecomesTerminal) {
    // Terminal-state contract: after RecordError with resynchronized=false,
    // the stream transitions to terminal. Subsequent calls return EndOfStream,
    // preventing retry loops on an unrecoverable error.
    //
    // The terminal state is enforced by an explicit failed_ latch (set on
    // RecordError, checked at entry), making the contract maeparser-independent.
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/corrupt_second_ct.mae");
    OEChem::OEMol mol;

    ASSERT_EQ(OEMaestro::ReadStatus::Ok, reader.TryRead(mol).status);

    const auto second = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::RecordError, second.status);
    EXPECT_FALSE(second.resynchronized);

    // After RecordError, the stream becomes terminal and returns EndOfStream.
    // The latch ensures this holds regardless of maeparser's internal behavior.
    EXPECT_EQ(OEMaestro::ReadStatus::EndOfStream, reader.TryRead(mol).status);
}

TEST(ReadStatus, RecordErrorLeavesMolUntouched) {
    // Contract: on RecordError, the OEMol is left untouched (still contains
    // the previous successful read). The caller can inspect it to see what
    // was last successfully parsed.
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/corrupt_second_ct.mae");
    OEChem::OEMol mol;

    const auto first = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::Ok, first.status);
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    const auto first_atoms = mol.NumAtoms();
    EXPECT_EQ(3u, first_atoms);

    const auto second = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::RecordError, second.status);

    // Mol should still contain "Ethanol" from the first read.
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    EXPECT_EQ(first_atoms, mol.NumAtoms());
}

TEST(ReadStatus, InvalidBondIndexFixtureThrowsWithRead) {
    // Verify the fixture: Read() should throw MaestroConvertError on the
    // second CT (bond referencing atom index 5 when only 2 atoms exist).
    // Also documents that Read() DOES contaminate the mol (unlike TryRead).
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/invalid_bond_index.mae");
    OEChem::OEGraphMol mol;

    ASSERT_TRUE(reader.Read(static_cast<OEChem::OEMolBase&>(mol)));
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    EXPECT_EQ(3u, mol.NumAtoms());

    // The throwing Read() path contaminates the mol (ConvertToOE clears and
    // partially populates before throwing).
    EXPECT_THROW(reader.Read(static_cast<OEChem::OEMolBase&>(mol)), OEMaestro::MaestroConvertError);
}

TEST(ReadStatus, ConversionErrorLeavesMolUntouched) {
    // Contract: when MolConverter throws (e.g. invalid bond index), the mol
    // is left untouched by TryRead. The fixture has a second CT that parses
    // successfully but declares a bond referencing atom index 5 when only 2
    // atoms exist, triggering MaestroConvertError after atoms are added.
    // With staging, TryRead catches the exception and leaves the caller's mol
    // untouched (still contains "Ethanol" from the first read).
    OEMaestro::OEMaestroReader reader(DATA_DIR + "/invalid_bond_index.mae");
    OEChem::OEMol mol;

    const auto first = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::Ok, first.status);
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    EXPECT_EQ(3u, mol.NumAtoms());

    const auto second = reader.TryRead(mol);
    ASSERT_EQ(OEMaestro::ReadStatus::RecordError, second.status);
    EXPECT_FALSE(second.message.empty());

    // Mol should still contain "Ethanol" from the first read (untouched by TryRead).
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    EXPECT_EQ(3u, mol.NumAtoms());
}

TEST(ReadStatus, ConversionErrorLeavesMolBaseUntouched) {
    // Contract: when MolConverter throws, the OEMolBase is left untouched.
    // This test uses ONLY TryRead(OEMolBase&) which calls Read(OEMolBase&) that
    // converts directly into the caller's mol WITHOUT staging (line 240 of
    // OEMaestroReader.cpp). Without explicit staging in TryRead, this test
    // FAILS with partial state contamination (title="InvalidBond", 2 atoms).
    //
    // Use TWO SEPARATE readers to avoid the failed_ latch preventing the
    // second read entirely.
    {
        OEMaestro::OEMaestroReader reader1(DATA_DIR + "/invalid_bond_index.mae");
        OEChem::OEGraphMol mol;
        const auto first = reader1.TryRead(static_cast<OEChem::OEMolBase&>(mol));
        ASSERT_EQ(OEMaestro::ReadStatus::Ok, first.status);
        EXPECT_STREQ("Ethanol", mol.GetTitle());
        EXPECT_EQ(3u, mol.NumAtoms());
    }

    {
        OEMaestro::OEMaestroReader reader2(DATA_DIR + "/invalid_bond_index.mae");
        OEChem::OEGraphMol mol;

        // Read first CT (Ethanol) successfully
        auto first = reader2.TryRead(static_cast<OEChem::OEMolBase&>(mol));
        ASSERT_EQ(OEMaestro::ReadStatus::Ok, first.status);
        EXPECT_STREQ("Ethanol", mol.GetTitle());
        EXPECT_EQ(3u, mol.NumAtoms());

        // Try to read second CT (InvalidBond) - should fail with conversion error
        const auto second = reader2.TryRead(static_cast<OEChem::OEMolBase&>(mol));
        ASSERT_EQ(OEMaestro::ReadStatus::RecordError, second.status);
        EXPECT_FALSE(second.message.empty());

        // With staging: mol still contains "Ethanol" from the first read (untouched).
        EXPECT_STREQ("Ethanol", mol.GetTitle());
        EXPECT_EQ(3u, mol.NumAtoms());
    }
}
