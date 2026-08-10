#include <gtest/gtest.h>

#ifdef OEMAESTRO_HAS_OEIO
#include <oeio/oeio.h>
#include <oemaestro/Enums.h>
#include <oechem.h>

#include <any>
#include <filesystem>
#include <string>
#include <vector>

static const std::string DATA_DIR = TEST_DATA_DIR;

namespace {

class OeioHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "oemaestro_oeio_test";
        std::filesystem::create_directories(tmp_dir_);
    }
    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }
    std::filesystem::path tmp_dir_;
};

TEST_F(OeioHandlerTest, HandlerRegistered) {
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto info = handler->info();
    EXPECT_EQ(info.name, "Maestro");
}

TEST_F(OeioHandlerTest, InfoExtensions) {
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto info = handler->info();
    auto& exts = info.extensions;
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".mae"), exts.end());
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".mae.gz"), exts.end());
    EXPECT_NE(std::find(exts.begin(), exts.end(), ".maegz"), exts.end());
}

TEST_F(OeioHandlerTest, InfoCapabilities) {
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto info = handler->info();
    EXPECT_TRUE(info.supports_read);
    EXPECT_TRUE(info.supports_write);
    EXPECT_FALSE(info.supports_threaded_read);
    EXPECT_FALSE(info.supports_threaded_write);
}

TEST_F(OeioHandlerTest, ReadMae) {
    auto range = oeio::read(DATA_DIR + "/simple.mae");
    int count = 0;
    for (auto& mol : range) {
        EXPECT_GT(mol.NumAtoms(), 0u);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, ReadMaeGz) {
    auto range = oeio::read(DATA_DIR + "/simple.mae.gz");
    int count = 0;
    for (auto& mol : range) {
        EXPECT_GT(mol.NumAtoms(), 0u);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, ReadMulti) {
    auto range = oeio::read(DATA_DIR + "/multi.mae");
    int count = 0;
    for (auto& mol : range) {
        count++;
    }
    EXPECT_EQ(count, 2);
}

TEST_F(OeioHandlerTest, ReadMaeGzLookup) {
    auto* handler = oeio::FormatRegistry::instance().lookup("input.mae.gz");
    ASSERT_NE(handler, nullptr);
    EXPECT_EQ(handler->info().name, "Maestro");
}

TEST_F(OeioHandlerTest, ReadMaegzLookup) {
    auto* handler = oeio::FormatRegistry::instance().lookup("input.maegz");
    ASSERT_NE(handler, nullptr);
    EXPECT_EQ(handler->info().name, "Maestro");
}

TEST_F(OeioHandlerTest, WriteAndReadBack) {
    auto out_path = (tmp_dir_ / "roundtrip.mae").string();

    {
        auto writer = oeio::write(out_path);
        OEChem::OEGraphMol mol;
        auto* c = mol.NewAtom(6);
        auto* o = mol.NewAtom(8);
        mol.NewBond(c, o, 2);
        float coords[] = {0.0f, 0.0f, 0.0f, 1.2f, 0.0f, 0.0f};
        mol.SetCoords(coords);
        mol.SetTitle("C=O");
        writer.append(mol);
        writer.close();
    }

    auto range = oeio::read(out_path);
    int count = 0;
    for (auto& mol : range) {
        EXPECT_EQ(std::string(mol.GetTitle()), "C=O");
        EXPECT_EQ(mol.NumAtoms(), 2u);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, WriteGz) {
    auto out_path = (tmp_dir_ / "roundtrip.mae.gz").string();

    {
        auto writer = oeio::write(out_path);
        OEChem::OEGraphMol mol;
        mol.NewAtom(6);
        mol.SetTitle("gzip_test");
        writer.append(mol);
        writer.close();
    }

    auto range = oeio::read(out_path);
    int count = 0;
    for (auto& mol : range) {
        EXPECT_EQ(std::string(mol.GetTitle()), "gzip_test");
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, CustomReaderConfig) {
    OEMaestro::OEMaestroReaderConfig cfg;
    cfg.SetTags(OEMaestro::TAG_NAME);
    cfg.SetPerception(OEMaestro::PERCEPTION_NONE);
    cfg.SetNumThreads(1);

    auto range = oeio::read(DATA_DIR + "/simple.mae", std::any(cfg));
    int count = 0;
    for (auto& mol : range) {
        EXPECT_GT(mol.NumAtoms(), 0u);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, BadConfigFallsBackToDefaults) {
    std::any bad_config = std::string("not a config struct");
    auto range = oeio::read(DATA_DIR + "/simple.mae", bad_config);
    int count = 0;
    for (auto& mol : range) {
        EXPECT_GT(mol.NumAtoms(), 0u);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, ReadIntoOEMolBase) {
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto source = handler->make_reader(DATA_DIR + "/simple.mae", std::any{});
    ASSERT_NE(source, nullptr);

    OEChem::OEGraphMol container;
    OEChem::OEMolBase& mol_base = container;
    int count = 0;
    while (source->next(mol_base)) {
        EXPECT_GT(mol_base.NumAtoms(), 0u);
        count++;
        container.Clear();
    }
    EXPECT_EQ(count, 1);
}

TEST_F(OeioHandlerTest, TryNextReportsRecordErrorAndClearsMol) {
    // Handler-layer contract: try_next calls mol.Clear() before TryRead, so on
    // RecordError the mol is CLEARED (not left with the previous successful read).
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto source = handler->make_reader(DATA_DIR + "/corrupt_second_ct.mae", std::any{});
    ASSERT_NE(source, nullptr);

    OEChem::OEGraphMol mol;

    // First read succeeds: Ethanol
    auto first = source->try_next(mol);
    ASSERT_EQ(oeio::ReadStatus::Ok, first.status);
    EXPECT_STREQ("Ethanol", mol.GetTitle());
    EXPECT_EQ(3u, mol.NumAtoms());

    // Second read fails: corrupt block. The mol should be CLEARED.
    auto second = source->try_next(mol);
    ASSERT_EQ(oeio::ReadStatus::RecordError, second.status);
    EXPECT_FALSE(second.message.empty());
    EXPECT_FALSE(second.resynchronized);
    EXPECT_EQ(0u, mol.NumAtoms());  // Cleared by try_next before TryRead
}

TEST_F(OeioHandlerTest, TryNextBecomesTerminalAfterRecordError) {
    // Terminal-state contract at the handler layer: after RecordError with
    // resynchronized=false, subsequent try_next returns EndOfStream.
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto source = handler->make_reader(DATA_DIR + "/corrupt_second_ct.mae", std::any{});
    ASSERT_NE(source, nullptr);

    OEChem::OEGraphMol mol;

    ASSERT_EQ(oeio::ReadStatus::Ok, source->try_next(mol).status);

    const auto second = source->try_next(mol);
    ASSERT_EQ(oeio::ReadStatus::RecordError, second.status);

    // Third call returns EndOfStream (stream is terminal).
    EXPECT_EQ(oeio::ReadStatus::EndOfStream, source->try_next(mol).status);
}

TEST_F(OeioHandlerTest, ConversionErrorClearedMolContract) {
    // Handler-layer contract: when TryRead hits a MolConverter exception
    // (e.g. invalid bond index), the handler's mol.Clear() at entry survives
    // because TryRead stages internally and never touches the caller's mol
    // on failure.
    auto* handler = oeio::FormatRegistry::instance().lookup("test.mae");
    ASSERT_NE(handler, nullptr);
    auto source = handler->make_reader(DATA_DIR + "/invalid_bond_index.mae", std::any{});
    ASSERT_NE(source, nullptr);

    OEChem::OEGraphMol mol;

    ASSERT_EQ(oeio::ReadStatus::Ok, source->try_next(mol).status);

    const auto second = source->try_next(mol);
    ASSERT_EQ(oeio::ReadStatus::RecordError, second.status);

    // The handler clears the mol before TryRead, and TryRead doesn't touch it
    // on failure, so the mol is empty (not partial state from the failed conversion).
    EXPECT_EQ(0u, mol.NumAtoms());
}

}  // namespace

#endif  // OEMAESTRO_HAS_OEIO
