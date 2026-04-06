#include <gtest/gtest.h>
#include "oemaestro/MolConverter.h"
#include "oemaestro/Error.h"
#include <oechem.h>

using namespace OEMaestro;

// Helper to build a simple 2-atom MaestroMol (C=O)
static MaestroMol make_simple_mol() {
    MaestroMol mm;
    mm.title = "TestMol";

    MaestroAtom c_atom;
    c_atom.atomic_number = 6;
    c_atom.x = 0.0;
    c_atom.y = 0.0;
    c_atom.z = 0.0;
    c_atom.formal_charge = 0;
    c_atom.atom_name = " C  ";
    c_atom.occupancy = 1.0;
    mm.atoms.push_back(c_atom);

    MaestroAtom o_atom;
    o_atom.atomic_number = 8;
    o_atom.x = 1.5;
    o_atom.y = 0.0;
    o_atom.z = 0.0;
    o_atom.formal_charge = 0;
    o_atom.atom_name = " O  ";
    o_atom.occupancy = 1.0;
    mm.atoms.push_back(o_atom);

    MaestroBond bond;
    bond.atom1_index = 0;
    bond.atom2_index = 1;
    bond.order = 2;
    mm.bonds.push_back(bond);

    return mm;
}

TEST(MolConverterTest, ConvertSimpleMolecule) {
    MolConverter conv;
    auto mm = make_simple_mol();
    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    EXPECT_EQ(mol.NumAtoms(), 2u);
    EXPECT_EQ(mol.NumBonds(), 1u);
    EXPECT_STREQ(mol.GetTitle(), "TestMol");
}

TEST(MolConverterTest, ConvertCoordinates) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();
    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    OESystem::OEIter<OEChem::OEAtomBase> ai = mol.GetAtoms();
    float xyz[3];
    mol.GetCoords(&(*ai), xyz);
    EXPECT_NEAR(xyz[0], 0.0, 0.001);
    EXPECT_NEAR(xyz[1], 0.0, 0.001);
    EXPECT_NEAR(xyz[2], 0.0, 0.001);
    ++ai;
    mol.GetCoords(&(*ai), xyz);
    EXPECT_NEAR(xyz[0], 1.5, 0.001);
    EXPECT_NEAR(xyz[1], 0.0, 0.001);
    EXPECT_NEAR(xyz[2], 0.0, 0.001);
}

TEST(MolConverterTest, ConvertResidueInfo) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    MaestroMol mm;
    mm.title = "ResTest";

    MaestroAtom n_atom;
    n_atom.atomic_number = 7;
    n_atom.x = 0.0;
    n_atom.y = 0.0;
    n_atom.z = 0.0;
    n_atom.formal_charge = 0;
    n_atom.atom_name = " N  ";
    n_atom.residue_name = "ALA ";
    n_atom.residue_number = 1;
    n_atom.chain_id = "A";
    n_atom.insert_code = " ";
    n_atom.bfactor = 20.5;
    n_atom.occupancy = 0.95;
    mm.atoms.push_back(n_atom);

    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    auto atom = mol.GetAtoms();
    OEChem::OEResidue res = OEChem::OEAtomGetResidue(*atom);
    EXPECT_STREQ(res.GetName(), "ALA ");
    EXPECT_EQ(res.GetResidueNumber(), 1);
    EXPECT_EQ(res.GetChainID(), 'A');
    EXPECT_NEAR(res.GetBFactor(), 20.5, 0.001);
    EXPECT_NEAR(res.GetOccupancy(), 0.95, 0.001);
}

TEST(MolConverterTest, TagFormatAll) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    MaestroMol mm;
    mm.title = "TagTest";

    MaestroAtom atom;
    atom.atomic_number = 6;
    mm.atoms.push_back(atom);
    mm.ct_properties["r_m_pdb_tfactor"] = "20.0";

    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    EXPECT_TRUE(mol.HasData("r_m_pdb_tfactor"));
    EXPECT_DOUBLE_EQ(mol.GetDoubleData("r_m_pdb_tfactor"), 20.0);
}

TEST(MolConverterTest, TagFormatNameOnly) {
    MolConverter conv(TAG_NAME, PERCEPTION_NONE);
    MaestroMol mm;
    mm.title = "TagTest";

    MaestroAtom atom;
    atom.atomic_number = 6;
    mm.atoms.push_back(atom);
    mm.ct_properties["r_m_pdb_tfactor"] = "20.0";

    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    EXPECT_TRUE(mol.HasData("pdb_tfactor"));
    EXPECT_DOUBLE_EQ(mol.GetDoubleData("pdb_tfactor"), 20.0);
    EXPECT_FALSE(mol.HasData("r_m_pdb_tfactor"));
}

TEST(MolConverterTest, TagFormatNone) {
    MolConverter conv(TAG_NONE, PERCEPTION_NONE);
    MaestroMol mm;
    mm.title = "TagTest";

    MaestroAtom atom;
    atom.atomic_number = 6;
    mm.atoms.push_back(atom);
    mm.ct_properties["r_m_pdb_tfactor"] = "20.0";

    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    // With TAG_NONE, no data tags should be stored
    bool has_any = false;
    for (auto dp = mol.GetDataIter(); dp; ++dp) {
        has_any = true;
    }
    EXPECT_FALSE(has_any);
}

TEST(MolConverterTest, TypedDataFromPrefix) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    MaestroMol mm;
    mm.title = "TypedTest";

    MaestroAtom atom;
    atom.atomic_number = 6;
    mm.atoms.push_back(atom);
    mm.ct_properties["i_m_ct_format"] = "2";
    mm.ct_properties["r_sd_MolWt"] = "180.156";
    mm.ct_properties["s_lp_Force_Field"] = "OPLS4";
    mm.ct_properties["b_sd_chiral_flag"] = "1";

    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);

    EXPECT_EQ(mol.GetIntData("i_m_ct_format"), 2);
    EXPECT_DOUBLE_EQ(mol.GetDoubleData("r_sd_MolWt"), 180.156);
    EXPECT_EQ(mol.GetStringData("s_lp_Force_Field"), "OPLS4");
    EXPECT_EQ(mol.GetIntData("b_sd_chiral_flag"), 1);
}

TEST(MolConverterTest, ConvertBondOrder) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();  // Has order=2 bond
    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    OESystem::OEIter<OEChem::OEBondBase> bi = mol.GetBonds();
    EXPECT_EQ(bi->GetOrder(), 2u);
}

TEST(MolConverterTest, ConvertTitle) {
    MolConverter conv;
    auto mm = make_simple_mol();
    mm.title = "My Molecule";
    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    EXPECT_STREQ(mol.GetTitle(), "My Molecule");
}

TEST(MolConverterTest, InvalidBondIndexThrows) {
    MolConverter conv;
    MaestroMol mm;

    MaestroAtom atom;
    atom.atomic_number = 6;
    mm.atoms.push_back(atom);

    MaestroBond bond;
    bond.atom1_index = 0;
    bond.atom2_index = 5;  // out of range
    bond.order = 1;
    mm.bonds.push_back(bond);

    OEChem::OEGraphMol mol;
    EXPECT_THROW(conv.Convert(mol, mm), MaestroConvertError);
}

TEST(MolConverterTest, PerceptionNone) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();
    OEChem::OEGraphMol mol;
    conv.Convert(mol, mm);
    EXPECT_EQ(mol.NumAtoms(), 2u);
    // Just verify it completes without error
}

TEST(MolConverterTest, SettersAndGetters) {
    MolConverter conv;
    conv.SetTagFormat(TAG_NAME);
    EXPECT_EQ(conv.GetTagFormat(), TAG_NAME);
    conv.SetPerception(PERCEPTION_NONE);
    EXPECT_EQ(conv.GetPerception(), PERCEPTION_NONE);
}
