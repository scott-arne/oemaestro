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
    conv.ConvertToOE(mol, mm);
    EXPECT_EQ(mol.NumAtoms(), 2u);
    EXPECT_EQ(mol.NumBonds(), 1u);
    EXPECT_STREQ(mol.GetTitle(), "TestMol");
}

TEST(MolConverterTest, DeduplicatesReverseListedBond) {
    // Maestro m_bond blocks can list the same bond in both directions (a->b and b->a).
    // The converter must produce a single OEBond, not a parallel duplicate.
    MaestroMol mm;
    mm.title = "DupBond";

    MaestroAtom a1;
    a1.atomic_number = 6;
    a1.occupancy = 1.0;
    mm.atoms.push_back(a1);

    MaestroAtom a2;
    a2.atomic_number = 6;
    a2.x = 1.5;
    a2.occupancy = 1.0;
    mm.atoms.push_back(a2);

    MaestroBond forward;
    forward.atom1_index = 0;
    forward.atom2_index = 1;
    forward.order = 1;
    mm.bonds.push_back(forward);

    MaestroBond reverse;  // same bond, opposite direction
    reverse.atom1_index = 1;
    reverse.atom2_index = 0;
    reverse.order = 1;
    mm.bonds.push_back(reverse);

    MolConverter conv;
    OEChem::OEGraphMol mol;
    conv.ConvertToOE(mol, mm);

    EXPECT_EQ(mol.NumAtoms(), 2u);
    EXPECT_EQ(mol.NumBonds(), 1u);
}

TEST(MolConverterTest, ConvertCoordinates) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();
    OEChem::OEGraphMol mol;
    conv.ConvertToOE(mol, mm);
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
    conv.ConvertToOE(mol, mm);
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
    conv.ConvertToOE(mol, mm);
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
    conv.ConvertToOE(mol, mm);
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
    conv.ConvertToOE(mol, mm);
    // With TAG_NONE, no ct_properties should be stored as generic data.
    // Note: GetDataIter() returns a raw OEIterBase* — must use explicit
    // OEIter<OEBaseData> type, not auto (auto deduces a raw pointer whose
    // bool test is always true, causing an infinite loop).
    bool has_any = false;
    for (OESystem::OEIter<OESystem::OEBaseData> gdata = mol.GetDataIter(); gdata; ++gdata) {
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
    conv.ConvertToOE(mol, mm);

    EXPECT_EQ(mol.GetIntData("i_m_ct_format"), 2);
    EXPECT_DOUBLE_EQ(mol.GetDoubleData("r_sd_MolWt"), 180.156);
    EXPECT_EQ(mol.GetStringData("s_lp_Force_Field"), "OPLS4");
    EXPECT_EQ(mol.GetIntData("b_sd_chiral_flag"), 1);
}

TEST(MolConverterTest, ConvertBondOrder) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();  // Has order=2 bond
    OEChem::OEGraphMol mol;
    conv.ConvertToOE(mol, mm);
    OESystem::OEIter<OEChem::OEBondBase> bi = mol.GetBonds();
    EXPECT_EQ(bi->GetOrder(), 2u);
}

TEST(MolConverterTest, ConvertTitle) {
    MolConverter conv;
    auto mm = make_simple_mol();
    mm.title = "My Molecule";
    OEChem::OEGraphMol mol;
    conv.ConvertToOE(mol, mm);
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
    EXPECT_THROW(conv.ConvertToOE(mol, mm), MaestroConvertError);
}

TEST(MolConverterTest, PerceptionNone) {
    MolConverter conv(TAG_ALL, PERCEPTION_NONE);
    auto mm = make_simple_mol();
    OEChem::OEGraphMol mol;
    conv.ConvertToOE(mol, mm);
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

// --- Write direction tests ---

TEST(MolConverterTest, WriteDirection_SingleAtom) {
    OEChem::OEGraphMol mol;
    auto* atom = mol.NewAtom(6);
    float coords[] = {1.0f, 2.0f, 3.0f};
    mol.SetCoords(atom, coords);
    mol.SetTitle("test");

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.title, "test");
    EXPECT_EQ(mmol.NumAtoms(), 1u);
    EXPECT_EQ(mmol.atoms[0].atomic_number, 6);
    EXPECT_NEAR(mmol.atoms[0].x, 1.0, 1e-4);
    EXPECT_NEAR(mmol.atoms[0].y, 2.0, 1e-4);
    EXPECT_NEAR(mmol.atoms[0].z, 3.0, 1e-4);
}

TEST(MolConverterTest, WriteDirection_BondsAndCharges) {
    OEChem::OEGraphMol mol;
    auto* c = mol.NewAtom(6);
    auto* o = mol.NewAtom(8);
    o->SetFormalCharge(-1);
    mol.NewBond(c, o, 2);
    mol.SetTitle("C=O");

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.NumAtoms(), 2u);
    EXPECT_EQ(mmol.NumBonds(), 1u);
    EXPECT_EQ(mmol.atoms[1].formal_charge, -1);
    EXPECT_EQ(mmol.bonds[0].atom1_index, 0);
    EXPECT_EQ(mmol.bonds[0].atom2_index, 1);
    EXPECT_EQ(mmol.bonds[0].order, 2);
}

TEST(MolConverterTest, WriteDirection_IsotopeAndPartialCharge) {
    OEChem::OEGraphMol mol;
    auto* atom = mol.NewAtom(6);
    atom->SetIsotope(13);
    atom->SetPartialCharge(0.25);

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.atoms[0].isotope, 13);
    EXPECT_NEAR(mmol.atoms[0].partial_charge, 0.25, 1e-6);
}

TEST(MolConverterTest, WriteDirection_ResidueInfo) {
    OEChem::OEGraphMol mol;
    auto* atom = mol.NewAtom(7);
    atom->SetName(" CA ");
    OEChem::OEResidue res;
    res.SetName("ALA");
    res.SetResidueNumber(42);
    res.SetChainID("A");
    res.SetInsertCode('B');
    res.SetBFactor(15.5);
    res.SetOccupancy(0.9);
    res.SetSecondaryStructure(OEBio::OESecondaryStructure::HelixAlpha);
    OEChem::OEAtomSetResidue(atom, res);

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.atoms[0].atom_name, " CA ");
    EXPECT_EQ(mmol.atoms[0].residue_name, "ALA");
    EXPECT_EQ(mmol.atoms[0].residue_number, 42);
    EXPECT_EQ(mmol.atoms[0].chain_id, "A");
    EXPECT_EQ(mmol.atoms[0].insert_code, "B");
    EXPECT_NEAR(mmol.atoms[0].bfactor, 15.5, 1e-4);
    EXPECT_NEAR(mmol.atoms[0].occupancy, 0.9, 1e-4);
    EXPECT_EQ(mmol.atoms[0].secondary_structure, 1);
}

TEST(MolConverterTest, WriteDirection_SDData) {
    OEChem::OEGraphMol mol;
    mol.NewAtom(6);
    mol.SetTitle("test");
    OEChem::OESetSDData(mol, "r_m_mol_weight", "44.01");
    OEChem::OESetSDData(mol, "s_user_myfield", "hello");

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.ct_properties.count("r_m_mol_weight"), 1u);
    EXPECT_EQ(mmol.ct_properties.at("r_m_mol_weight"), "44.01");
    EXPECT_EQ(mmol.ct_properties.count("s_user_myfield"), 1u);
    EXPECT_EQ(mmol.ct_properties.at("s_user_myfield"), "hello");
}

TEST(MolConverterTest, WriteDirection_EmptyMolecule) {
    OEChem::OEGraphMol mol;
    mol.SetTitle("empty");

    MolConverter converter;
    MaestroMol mmol;
    converter.ConvertToMaestro(mmol, mol);

    EXPECT_EQ(mmol.title, "empty");
    EXPECT_EQ(mmol.NumAtoms(), 0u);
    EXPECT_EQ(mmol.NumBonds(), 0u);
}

TEST(MolConverterTest, WriteDirection_MultiConformer) {
    OEChem::OEMol mol;
    auto* c = mol.NewAtom(6);
    auto* o = mol.NewAtom(8);
    mol.NewBond(c, o, 2);
    mol.SetTitle("conformers");

    float coords1[] = {0.0f, 0.0f, 0.0f, 1.2f, 0.0f, 0.0f};
    mol.SetCoords(coords1);
    float coords2[] = {0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 0.0f};
    mol.NewConf(coords2);

    MolConverter converter;
    std::vector<MaestroMol> mmols;
    converter.ConvertToMaestro(mmols, mol);

    EXPECT_EQ(mmols.size(), 2u);
    EXPECT_EQ(mmols[0].NumAtoms(), 2u);
    EXPECT_EQ(mmols[1].NumAtoms(), 2u);
    EXPECT_EQ(mmols[0].NumBonds(), 1u);
    EXPECT_NEAR(mmols[0].atoms[1].x, 1.2, 1e-4);
    EXPECT_NEAR(mmols[0].atoms[1].y, 0.0, 1e-4);
    EXPECT_NEAR(mmols[1].atoms[1].x, 0.0, 1e-4);
    EXPECT_NEAR(mmols[1].atoms[1].y, 1.2, 1e-4);
}
