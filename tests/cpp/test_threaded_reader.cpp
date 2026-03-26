#include <gtest/gtest.h>
#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/OEReadMaestro.h"
#include "oemaestro/Error.h"
#include <oechem.h>
#include <vector>
#include <string>

static const std::string DATA_DIR = TEST_DATA_DIR;

static std::vector<std::string> read_all_titles(const std::string& path,
                                                 OEMaestro::OEMaestroReaderConfig cfg) {
    OEMaestro::OEMaestroReader reader(path, cfg);
    OEChem::OEMol mol;
    std::vector<std::string> titles;
    while (reader.Read(mol)) {
        titles.push_back(mol.GetTitle());
    }
    return titles;
}

static std::vector<unsigned int> read_all_atom_counts(const std::string& path,
                                                       OEMaestro::OEMaestroReaderConfig cfg) {
    OEMaestro::OEMaestroReader reader(path, cfg);
    OEChem::OEMol mol;
    std::vector<unsigned int> counts;
    while (reader.Read(mol)) {
        counts.push_back(mol.NumAtoms());
    }
    return counts;
}

TEST(ThreadedReaderTest, IdenticalResultsMultiFile) {
    std::string path = DATA_DIR + "/multi.mae";
    OEMaestro::OEMaestroReaderConfig cfg1;
    cfg1.num_threads = 1;
    OEMaestro::OEMaestroReaderConfig cfg4;
    cfg4.num_threads = 4;

    auto titles1 = read_all_titles(path, cfg1);
    auto titles4 = read_all_titles(path, cfg4);
    ASSERT_EQ(titles1.size(), titles4.size());
    for (size_t i = 0; i < titles1.size(); i++) {
        EXPECT_EQ(titles1[i], titles4[i]) << "Mismatch at index " << i;
    }

    auto counts1 = read_all_atom_counts(path, cfg1);
    auto counts4 = read_all_atom_counts(path, cfg4);
    ASSERT_EQ(counts1.size(), counts4.size());
    for (size_t i = 0; i < counts1.size(); i++) {
        EXPECT_EQ(counts1[i], counts4[i]) << "Mismatch at index " << i;
    }
}

TEST(ThreadedReaderTest, IdenticalResultsProtein) {
    std::string path = DATA_DIR + "/protein.mae";
    OEMaestro::OEMaestroReaderConfig cfg1;
    cfg1.num_threads = 1;
    OEMaestro::OEMaestroReaderConfig cfg4;
    cfg4.num_threads = 4;

    auto titles1 = read_all_titles(path, cfg1);
    auto titles4 = read_all_titles(path, cfg4);
    EXPECT_EQ(titles1, titles4);
}

TEST(ThreadedReaderTest, OrderPreserved) {
    std::string path = DATA_DIR + "/multi.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 4;

    auto titles = read_all_titles(path, cfg);
    ASSERT_EQ(titles.size(), 2u);
    OEMaestro::OEMaestroReaderConfig cfg1;
    cfg1.num_threads = 1;
    auto titles1 = read_all_titles(path, cfg1);
    EXPECT_EQ(titles[0], titles1[0]);
    EXPECT_EQ(titles[1], titles1[1]);
}

TEST(ThreadedReaderTest, ReadMolBase) {
    std::string path = DATA_DIR + "/multi.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 4;
    OEMaestro::OEMaestroReader reader(path, cfg);
    OEChem::OEGraphMol mol;
    int count = 0;
    while (reader.Read(static_cast<OEChem::OEMolBase&>(mol))) count++;
    EXPECT_EQ(count, 2);
}

TEST(ThreadedReaderTest, MoreThreadsThanCTs) {
    std::string path = DATA_DIR + "/simple.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 8;
    OEMaestro::OEMaestroReader reader(path, cfg);
    OEChem::OEMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9u);
    ASSERT_FALSE(reader.Read(mol));
}

TEST(ThreadedReaderTest, EarlyDestruction) {
    std::string path = DATA_DIR + "/multi.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 4;
    {
        OEMaestro::OEMaestroReader reader(path, cfg);
        OEChem::OEMol mol;
        reader.Read(mol);
    }
    SUCCEED();
}

TEST(ThreadedReaderTest, SetPerceptionThrowsWhenThreaded) {
    std::string path = DATA_DIR + "/simple.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 4;
    OEMaestro::OEMaestroReader reader(path, cfg);
    EXPECT_THROW(reader.SetPerception(OEMaestro::PERCEPTION_NONE), std::logic_error);
    EXPECT_THROW(reader.SetTagFormat(OEMaestro::TAG_NAME), std::logic_error);
}

TEST(ThreadedReaderTest, SingleThreadUnchanged) {
    std::string path = DATA_DIR + "/simple.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 1;
    OEMaestro::OEMaestroReader reader(path, cfg);
    OEChem::OEMol mol;
    ASSERT_TRUE(reader.Read(mol));
    EXPECT_EQ(mol.NumAtoms(), 9u);
    EXPECT_STREQ(mol.GetTitle(), "Ethanol");
    EXPECT_NO_THROW(reader.SetPerception(OEMaestro::PERCEPTION_NONE));
}

TEST(ThreadedReaderTest, ConformerGroupingThreaded) {
    std::string path = DATA_DIR + "/conformers.mae";
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.num_threads = 4;
    OEMaestro::OEMaestroReader reader(path, cfg);
    reader.SetConfTest(new OEChem::OEIsomericConfTest());
    OEChem::OEMol mol;
    int count = 0;
    while (reader.Read(mol)) count++;
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mol.NumConfs(), 2);
}
