#include <gtest/gtest.h>
#include "oemaestro/OEMaestroTagConverter.h"

using namespace OEMaestro;

// --- IsFullMaestroKey ---

TEST(OEMaestroTagConverter, IsFullMaestroKey_FullKey) {
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("r_m_x_coord"));
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("i_m_atomic_number"));
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("s_m_pdb_atom_name"));
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("b_m_is_aromatic"));
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("r_user_myenergy"));
    EXPECT_TRUE(OEMaestroTagConverter::IsFullMaestroKey("r_i_glide_gscore"));
}

TEST(OEMaestroTagConverter, IsFullMaestroKey_NotFull) {
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey("x_coord"));
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey("m_x_coord"));
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey(""));
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey("ab"));
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey("x_"));
    // 'z' is not a valid type prefix
    EXPECT_FALSE(OEMaestroTagConverter::IsFullMaestroKey("z_m_foo"));
}

// --- ToFormatted ---

TEST(OEMaestroTagConverter, ToFormatted_TagAll) {
    OEMaestroTagConverter conv(TAG_ALL);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "r_m_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToFormatted_TagName) {
    OEMaestroTagConverter conv(TAG_NAME);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToFormatted_TagTypeAndName) {
    OEMaestroTagConverter conv(TAG_TYPE | TAG_NAME);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "r_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToFormatted_TagOwnerAndName) {
    OEMaestroTagConverter conv(TAG_OWNER | TAG_NAME);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "m_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToFormatted_TagTypeAndOwner) {
    OEMaestroTagConverter conv(TAG_TYPE | TAG_OWNER);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "r_m");
}

TEST(OEMaestroTagConverter, ToFormatted_TagNone) {
    OEMaestroTagConverter conv(TAG_NONE);
    EXPECT_EQ(conv.ToFormatted("r_m_pdb_tfactor"), "");
}

TEST(OEMaestroTagConverter, ToFormatted_NonMaestroKey) {
    OEMaestroTagConverter conv(TAG_NAME);
    // Keys that don't match t_o_d pattern are returned as-is
    EXPECT_EQ(conv.ToFormatted("custom_key"), "custom_key");
}

// --- ToMaestroTag ---

TEST(OEMaestroTagConverter, ToMaestroTag_AlreadyFull) {
    OEMaestroTagConverter conv;
    EXPECT_EQ(conv.ToMaestroTag("r_m_pdb_tfactor"), "r_m_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToMaestroTag_NameOnly_WithTypeHint) {
    OEMaestroTagConverter conv(TAG_ALL, "user");
    EXPECT_EQ(conv.ToMaestroTag("pdb_tfactor", 'r'), "r_user_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToMaestroTag_OwnerAndName_WithTypeHint) {
    OEMaestroTagConverter conv;
    EXPECT_EQ(conv.ToMaestroTag("m_pdb_tfactor", 'r'), "r_m_pdb_tfactor");
}

TEST(OEMaestroTagConverter, ToMaestroTag_NameOnly_NoTypeHint) {
    OEMaestroTagConverter conv(TAG_ALL, "user");
    // No type hint → defaults to 's' (string)
    EXPECT_EQ(conv.ToMaestroTag("myfield"), "s_user_myfield");
}

TEST(OEMaestroTagConverter, ToMaestroTag_CustomDefaultOwner) {
    OEMaestroTagConverter conv(TAG_ALL, "glide");
    EXPECT_EQ(conv.ToMaestroTag("gscore", 'r'), "r_glide_gscore");
}
