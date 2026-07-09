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
        dst.NewBond(
            atom_ptrs[static_cast<size_t>(bond.atom1_index)],
            atom_ptrs[static_cast<size_t>(bond.atom2_index)],
            static_cast<unsigned int>(bond.order));
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
