#include "oemaestro/MolConverter.h"
#include "oemaestro/Error.h"
#include "oemaestro/OEMaestroTagConverter.h"

#include <oechem.h>
#include <cerrno>
#include <string>
#include <unordered_map>
#include <vector>

namespace OEMaestro {

/// Map Maestro i_m_secondary_structure values to OpenEye constants.
/// Maestro: 0=loop/coil, 1=helix, 2=strand/sheet.
static int MaestroSSToOE(const int maestro_ss) {
    switch (maestro_ss) {
        case 1:  return OEBio::OESecondaryStructure::HelixAlpha;
        case 2:  return OEBio::OESecondaryStructure::Sheet;
        default: return OEBio::OESecondaryStructure::Unassigned;
    }
}

MolConverter::MolConverter()
    : tag_converter_(TAG_ALL), perception_(PERCEPTION_ALL) {}

// ReSharper disable CppParameterMayBeConst
MolConverter::MolConverter(OEMaestroTag tags, OEMaestroPerception perception)
// ReSharper restore CppParameterMayBeConst
    : tag_converter_(tags), perception_(perception) {}

void MolConverter::ConvertToOE(OEChem::OEMolBase& dst, const MaestroMol& src) const {
    dst.Clear();
    dst.SetTitle(src.title.c_str());

    std::vector<OEChem::OEAtomBase*> atom_ptrs;
    atom_ptrs.reserve(src.atoms.size());

    for (const auto& maestro_atom : src.atoms) {
        auto* oeatom = dst.NewAtom(static_cast<unsigned int>(maestro_atom.atomic_number));
        oeatom->SetName(maestro_atom.atom_name.c_str());
        oeatom->SetFormalCharge(maestro_atom.formal_charge);

        if (maestro_atom.isotope != 0)
            oeatom->SetIsotope(maestro_atom.isotope);
        if (maestro_atom.partial_charge != 0.0)
            oeatom->SetPartialCharge(maestro_atom.partial_charge);

        float xyz[3] = {
            static_cast<float>(maestro_atom.x),
            static_cast<float>(maestro_atom.y),
            static_cast<float>(maestro_atom.z)
        };
        dst.SetCoords(oeatom, xyz);

        OEChem::OEResidue res;
        res.SetName(maestro_atom.residue_name.c_str());
        res.SetResidueNumber(maestro_atom.residue_number);
        res.SetChainID(maestro_atom.chain_id.c_str());  // NOLINT(readability-redundant-string-cstr)
        res.SetInsertCode(maestro_atom.insert_code.empty() ? ' ' : maestro_atom.insert_code[0]);
        res.SetBFactor(static_cast<float>(maestro_atom.bfactor));
        res.SetOccupancy(static_cast<float>(maestro_atom.occupancy));
        if (maestro_atom.secondary_structure >= 0) {
            res.SetSecondaryStructure(MaestroSSToOE(maestro_atom.secondary_structure));
        }
        OEChem::OEAtomSetResidue(oeatom, res);

        if (maestro_atom.is_ligand_atom) {
            oeatom->SetBoolData("is_ligand_atom", true);
        }

        atom_ptrs.push_back(oeatom);
    }

    for (const auto& bond : src.bonds) {
        if (bond.atom1_index < 0 ||
            static_cast<size_t>(bond.atom1_index) >= atom_ptrs.size() ||
            bond.atom2_index < 0 ||
            static_cast<size_t>(bond.atom2_index) >= atom_ptrs.size()) {
            throw MaestroConvertError(
                "Bond index out of range: atom1=" + std::to_string(bond.atom1_index) +
                " atom2=" + std::to_string(bond.atom2_index) +
                " num_atoms=" + std::to_string(atom_ptrs.size()));
        }
        OEChem::OEAtomBase* atom1 = atom_ptrs[static_cast<size_t>(bond.atom1_index)];
        OEChem::OEAtomBase* atom2 = atom_ptrs[static_cast<size_t>(bond.atom2_index)];

        // Maestro m_bond blocks can list the same bond in both directions (a->b and b->a).
        // OpenEye represents a bond once, so skip the reverse-duplicate entry; creating a
        // parallel OEBond otherwise serializes to invalid SD/MOL bond blocks that OEChem
        // itself cannot read back.
        if (dst.GetBond(atom1, atom2))
            continue;

        dst.NewBond(atom1, atom2, static_cast<unsigned int>(bond.order));
    }

    auto dimension = static_cast<int>(OEChem::OEGetDimensionFromCoords(dst));
    dst.SetDimension(dimension);

    if (tag_converter_.GetTags() != TAG_NONE) {
        ApplyDataTags(src, dst);
    }

    RunPerception(dst, dimension);
}

void MolConverter::SetTagFormat(OEMaestroTag tags) {
    tag_converter_.SetTags(tags);
}

OEMaestroTag MolConverter::GetTagFormat() const {
    return tag_converter_.GetTags();
}

void MolConverter::SetPerception(OEMaestroPerception perception) {
    perception_ = perception;
}

OEMaestroPerception MolConverter::GetPerception() const {
    return perception_;
}

/// Extract the Maestro type prefix character from a key (e.g. 'i' from "i_m_ct_format").
/// Returns '\0' if the key does not match the t_o_d pattern.
static char maestro_type_prefix(const std::string& key) {
    if (key.size() >= 4 && key[1] == '_') return key[0];
    return '\0';
}

/// Set typed data on an OEBase object based on the Maestro type prefix.
static void set_typed_data(OESystem::OEBase& obj, const char* tag,
                           const std::string& val, char type_prefix) {
    switch (type_prefix) {
        case 'i': {
            errno = 0;
            char* end = nullptr;
            long lv = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str() && *end == '\0' && errno == 0) {
                obj.SetIntData(tag, static_cast<int>(lv));
                return;
            }
            break;
        }
        case 'r': {
            errno = 0;
            char* end = nullptr;
            double dv = std::strtod(val.c_str(), &end);
            if (end != val.c_str() && *end == '\0' && errno == 0) {
                obj.SetDoubleData(tag, dv);
                return;
            }
            break;
        }
        case 'b': {
            errno = 0;
            char* end = nullptr;
            long bv = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str() && *end == '\0' && errno == 0) {
                obj.SetIntData(tag, static_cast<int>(bv));
                return;
            }
            break;
        }
        default:
            break;
    }
    obj.SetStringData(tag, val);
}

void MolConverter::ApplyDataTags(const MaestroMol& maestro_mol,
                                  OEChem::OEMolBase& mol) const {
    std::unordered_map<std::string, std::string> tag_cache;
    auto format_tag = [&](const std::string& key) -> const std::string& {
        auto [it, inserted] = tag_cache.try_emplace(key);
        if (inserted) it->second = tag_converter_.ToFormatted(key);
        return it->second;
    };

    // CT-level properties -> typed generic data on molecule
    for (const auto& [key, val] : maestro_mol.ct_properties) {
        const std::string& tag = format_tag(key);
        set_typed_data(mol, tag.c_str(), val, maestro_type_prefix(key));
    }

    // Atom-level properties -> typed generic data on atoms
    for (OESystem::OEIter<OEChem::OEAtomBase> ai = mol.GetAtoms(); ai; ++ai) {
        size_t idx = ai->GetIdx();  // NOLINT
        if (idx < maestro_mol.atoms.size()) {
            for (const auto& [key, val] : maestro_mol.atoms[idx].properties) {
                const std::string& tag = format_tag(key);
                set_typed_data(*ai, tag.c_str(), val, maestro_type_prefix(key));
            }
        }
    }
}

void MolConverter::RunPerception(OEChem::OEMolBase& mol, int dimension) const {
    if ((perception_ & PERCEPTION_CONNECTIVITY) && dimension == 3) {
        OEChem::OEDetermineConnectivity(mol);
    }
    if (perception_ & PERCEPTION_RINGS) {
        OEChem::OEFindRingAtomsAndBonds(mol);
    }
    if (perception_ & PERCEPTION_BOND_ORDERS) {
        OEChem::OEPerceiveBondOrders(mol);
    }
    if (perception_ & PERCEPTION_IMPLICIT_HYDROGENS) {
        OEChem::OEAssignImplicitHydrogens(mol);
    }
    if (perception_ & PERCEPTION_FORMAL_CHARGES) {
        OEChem::OEAssignFormalCharges(mol);
    }
}

namespace {

/// Map OE secondary structure enum to Maestro integer.
int OESSToMaestro(unsigned int oe_ss) {
    switch (oe_ss) {
        case OEBio::OESecondaryStructure::HelixAlpha: return 1;
        case OEBio::OESecondaryStructure::Sheet:      return 2;
        default: return -1;
    }
}

/// Maestro "Element" color-scheme color for an atom, as a 6-digit uppercase
/// hex RGB string (e.g. "808080" for carbon).
///
/// Maestro renders atoms magenta when a structure carries no per-atom color.
/// OpenEye molecules have no Maestro color, so every converted atom is given
/// its element color via s_m_color_rgb. These values are Maestro's own
/// Element-scheme colors, captured by round-tripping a structure containing one
/// atom of every element (Z 1-118) through Maestro. Elements Maestro colors by
/// group rather than uniquely (noble gases, alkali/alkaline-earth metals, the
/// post-transition/metalloid block, the superheavy elements, ...) reproduce
/// that shared color exactly. Atoms with no recognized element (e.g. dummy
/// atoms, Z 0) fall back to a neutral gray so none is ever left color-less.
const char* MaestroElementColorRGB(int atomic_number) {
    // Indexed by atomic number; index 0 is the fallback color.
    static const char* const kColors[] = {
        "A0A0A0",  // 0  fallback (dummy / unknown)
        "FFFFFF", "FF6BB5", "FF6B6B", "FF6BFF", "2EFF2E", "808080",  // 1-6: H He Li Be B C
        "2E2EFF", "FF2E2E", "6BFFB5", "FF6BB5", "FF6B6B", "FF6BFF",  // 7-12: N O F Ne Na Mg
        "FFCB2E", "FF962E", "CC0066", "FFFF6B", "008C00", "FF6BB5",  // 13-18: Al Si P S Cl Ar
        "FF6B6B", "FF6BFF", "E6E6E6", "BFC2C7", "A6A6AB", "8A99C7",  // 19-24: K Ca Sc Ti V Cr
        "9C7AC7", "E54D00", "4D33CC", "00CC66", "CC4D1A", "7D80B0",  // 25-30: Mn Fe Co Ni Cu Zn
        "FFCB2E", "FFCB2E", "FF906B", "FF906B", "8C0000", "FF6BB5",  // 31-36: Ga Ge As Se Br Kr
        "FF6B6B", "FF6BFF", "94FFFF", "94E0E0", "73C2C9", "54B5B5",  // 37-42: Rb Sr Y Zr Nb Mo
        "3B9E9E", "248F8F", "0A7D8C", "006985", "C0C0C0", "FFD98F",  // 43-48: Tc Ru Rh Pd Ag Cd
        "FFCB2E", "FFCB2E", "FFCB2E", "FF906B", "FF2EFF", "FF6BB5",  // 49-54: In Sn Sb Te I Xe
        "FF6B6B", "FF6BFF", "70D4FF", "FFFFC7", "D9FFC7", "C7FFC7",  // 55-60: Cs Ba La Ce Pr Nd
        "A3FFC7", "8FFFC7", "61FFC7", "45FFC7", "30FFC7", "1FFFC7",  // 61-66: Pm Sm Eu Gd Tb Dy
        "00FF9C", "00E675", "00D452", "00BF38", "00AB24", "4DC2FF",  // 67-72: Ho Er Tm Yb Lu Hf
        "4DA6FF", "2194D6", "267DAB", "266696", "175487", "D0D0E0",  // 73-78: Ta W Re Os Ir Pt
        "FFD123", "B8B8D0", "FFCB2E", "FFCB2E", "FFCB2E", "FFCB2E",  // 79-84: Au Hg Tl Pb Bi Po
        "FF906B", "FF6BB5", "FF6B6B", "FF6BFF", "70ABFA", "00BAFF",  // 85-90: At Rn Fr Ra Ac Th
        "00A1FF", "008FFF", "0080FF", "006BFF", "545CF2", "785CE3",  // 91-96: Pa U Np Pu Am Cm
        "8A4FE3", "A136D4", "B31FD4", "B31FBA", "B30DA6", "B30DA6",  // 97-102: Bk Cf Es Fm Md No
        "C70066", "404040", "404040", "404040", "E11EE1", "E11EE1",  // 103-108: Lr Rf Db Sg Bh Hs
        "E11EE1", "E11EE1", "E11EE1", "E11EE1", "E11EE1", "E11EE1",  // 109-114: Mt Ds Rg Cn Nh Fl
        "E11EE1", "E11EE1", "E11EE1", "E11EE1",  // 115-118: Mc Lv Ts Og
    };
    if (atomic_number >= 1 && atomic_number <= 118)
        return kColors[atomic_number];
    return kColors[0];
}

/// Populate a MaestroMol from an OEMolBase.
/// If conf_coords is not nullptr, use those coordinates instead of mol.GetCoords().
void PopulateMaestroMol(MaestroMol& dst, const OEChem::OEMolBase& src,
                        const float* conf_coords,
                        const OEMaestroTagConverter& tag_converter) {
    dst.Clear();
    dst.title = src.GetTitle();

    // Atoms
    dst.atoms.reserve(src.GetMaxAtomIdx());
    for (OESystem::OEIter<OEChem::OEAtomBase> ai = src.GetAtoms(); ai; ++ai) {
        MaestroAtom matom;
        matom.atomic_number = static_cast<int>(ai->GetAtomicNum());

        unsigned int idx = ai->GetIdx();
        if (conf_coords) {
            matom.x = conf_coords[idx * 3];
            matom.y = conf_coords[idx * 3 + 1];
            matom.z = conf_coords[idx * 3 + 2];
        } else {
            float xyz[3];
            src.GetCoords(&(*ai), xyz);
            matom.x = xyz[0];
            matom.y = xyz[1];
            matom.z = xyz[2];
        }

        matom.formal_charge = ai->GetFormalCharge();
        matom.isotope = static_cast<int>(ai->GetIsotope());
        matom.partial_charge = ai->GetPartialCharge();
        matom.atom_name = ai->GetName();

        OEChem::OEResidue res = OEChem::OEAtomGetResidue(&(*ai));
        std::string res_name = res.GetName();
        if (!res_name.empty()) {
            matom.residue_name = res_name;
            matom.residue_number = res.GetResidueNumber();
            char chain = res.GetChainID();
            if (chain != ' ' && chain != '\0')
                matom.chain_id = std::string(1, chain);
            char icode = res.GetInsertCode();
            if (icode != ' ' && icode != '\0')
                matom.insert_code = std::string(1, icode);
            matom.bfactor = res.GetBFactor();
            matom.occupancy = res.GetOccupancy();
            matom.secondary_structure = OESSToMaestro(res.GetSecondaryStructure());
        }

        if (ai->GetBoolData("is_ligand_atom"))
            matom.is_ligand_atom = true;

        // Give every atom its Maestro Element-scheme color. Without a per-atom
        // color, Maestro displays imported structures entirely in magenta.
        matom.properties["s_m_color_rgb"] = MaestroElementColorRGB(matom.atomic_number);

        dst.atoms.push_back(std::move(matom));
    }

    // Bonds
    dst.bonds.reserve(src.GetMaxBondIdx());
    for (OESystem::OEIter<OEChem::OEBondBase> bi = src.GetBonds(); bi; ++bi) {
        MaestroBond mbond;
        mbond.atom1_index = static_cast<int>(bi->GetBgnIdx());
        mbond.atom2_index = static_cast<int>(bi->GetEndIdx());
        mbond.order = static_cast<int>(bi->GetOrder());
        dst.bonds.push_back(mbond);
    }

    // CT-level generic data — extract SD data pairs and typed generic data.
    // SD data (set via OESetSDData) is accessible via OEGetSDDataPairs.
    for (OESystem::OEIter<OEChem::OESDDataPair> dp = OEChem::OEGetSDDataPairs(src); dp; ++dp) {
        std::string tag_str = dp->GetTag();
        if (tag_str.empty()) continue;

        char type_prefix = '\0';
        if (OEMaestroTagConverter::IsFullMaestroKey(tag_str)) {
            type_prefix = tag_str[0];
        }

        std::string maestro_key = tag_converter.ToMaestroTag(tag_str, type_prefix);
        dst.ct_properties[maestro_key] = dp->GetValue();
    }

    // Note: Typed generic data (SetIntData/SetDoubleData/SetStringData)
    // is not iterable via the OE API. Only SD data pairs are captured.
    // For full round-trip of CT properties, use the Layer 1 MaestroWriter
    // with MaestroMol directly.
}

}  // anonymous namespace

void MolConverter::ConvertToMaestro(MaestroMol& dst, const OEChem::OEMolBase& src) const {
    PopulateMaestroMol(dst, src, nullptr, tag_converter_);
}

void MolConverter::ConvertToMaestro(std::vector<MaestroMol>& dst,
                                    const OEChem::OEMolBase& src) const {
    dst.clear();

    // Try to cast to OEMCMolBase to access conformers.
    // Note: dynamic_cast<const OEMol*> fails with static OpenEye libraries,
    // but OEMCMolBase cast works and provides GetConfs()/NumConfs().
    // ReSharper disable CppTooWideScopeInitStatement
    const OEChem::OEMCMolBase* mc_ptr = dynamic_cast<const OEChem::OEMCMolBase*>(&src);  // NOLINT(modernize-use-auto)
    // ReSharper restore CppTooWideScopeInitStatement
    if (mc_ptr && mc_ptr->NumConfs() > 1) {
        for (OESystem::OEIter<OEChem::OEConfBase> ci = mc_ptr->GetConfs(); ci; ++ci) {
            MaestroMol mmol;
            std::vector<float> coords(src.GetMaxAtomIdx() * 3);
            ci->GetCoords(coords.data());
            PopulateMaestroMol(mmol, src, coords.data(), tag_converter_);
            dst.push_back(std::move(mmol));
        }
        return;
    }

    // No conformers or not an OEMol: write active conformer only
    dst.emplace_back();
    PopulateMaestroMol(dst.back(), src, nullptr, tag_converter_);
}

}  // namespace OEMaestro
