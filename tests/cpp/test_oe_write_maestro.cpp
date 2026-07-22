#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <oechem.h>
#include "oemaestro/OEWriteMaestro.h"
#include "oemaestro/OEReadMaestro.h"


using namespace OEMaestro;
namespace fs = std::filesystem;

class OEWriteMaestroTest : public ::testing::Test {
protected:
    fs::path tmp_dir_;
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "oemaestro_write_fn_test";
        fs::create_directories(tmp_dir_);
    }
    void TearDown() override {
        fs::remove_all(tmp_dir_);
    }
};

TEST_F(OEWriteMaestroTest, SingleMolWrite) {
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);
    mol.SetTitle("free_fn_test");

    auto path = (tmp_dir_ / "free_fn.mae").string();
    EXPECT_TRUE(OEWriteMaestro(path, mol));

    OEChem::OEGraphMol read_mol;
    EXPECT_TRUE(OEReadMaestro(path, read_mol));
    EXPECT_EQ(std::string(read_mol.GetTitle()), "free_fn_test");
}

TEST_F(OEWriteMaestroTest, WritesElementColorRGB) {
    // Regression: without per-atom color, Maestro displays all atoms magenta.
    // The written .mae must carry s_m_color_rgb with element colors.
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);   // carbon
    mol.NewAtom(8);   // oxygen
    mol.SetTitle("colored");

    auto path = (tmp_dir_ / "colored.mae").string();
    ASSERT_TRUE(OEWriteMaestro(path, mol));

    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("s_m_color_rgb"), std::string::npos);
    EXPECT_NE(text.find("808080"), std::string::npos);  // carbon gray
    EXPECT_NE(text.find("FF2E2E"), std::string::npos);  // oxygen red
}

TEST_F(OEWriteMaestroTest, WithConfig) {
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);
    mol.SetTitle("config_test");

    OEMaestroWriterConfig config;
    config.SetTags(TAG_ALL);

    auto path = (tmp_dir_ / "config.mae").string();
    EXPECT_TRUE(OEWriteMaestro(path, mol, config));

    OEChem::OEGraphMol read_mol;
    EXPECT_TRUE(OEReadMaestro(path, read_mol));
    EXPECT_EQ(std::string(read_mol.GetTitle()), "config_test");
}
