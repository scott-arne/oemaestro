#include "oemaestro/MolConverter.h"
#include "oemaestro/Error.h"

#include <oechem.h>
#include <string>
#include <vector>

namespace OEMaestro {

MolConverter::MolConverter()
    : tags_(TAG_ALL), perception_(PERCEPTION_ALL) {}

MolConverter::MolConverter(OEMaestroTag tags, OEMaestroPerception perception)
    : tags_(tags), perception_(perception) {}

void MolConverter::Convert(const MaestroMol& maestro_mol, OEChem::OEMolBase& mol) const {
    mol.Clear();
    mol.SetTitle(maestro_mol.title.c_str());

    std::vector<OEChem::OEAtomBase*> atom_ptrs;
    atom_ptrs.reserve(maestro_mol.atoms.size());

    for (const auto& atom : maestro_mol.atoms) {
        auto* oeatom = mol.NewAtom(static_cast<unsigned int>(atom.atomic_number));
        oeatom->SetName(atom.atom_name.c_str());
        oeatom->SetFormalCharge(atom.formal_charge);

        float xyz[3] = {
            static_cast<float>(atom.x),
            static_cast<float>(atom.y),
            static_cast<float>(atom.z)
        };
        mol.SetCoords(oeatom, xyz);

        OEChem::OEResidue res;
        res.SetName(atom.residue_name.c_str());
        res.SetResidueNumber(atom.residue_number);
        res.SetChainID(atom.chain_id.c_str());
        res.SetInsertCode(atom.insert_code.empty() ? ' ' : atom.insert_code[0]);
        res.SetBFactor(atom.bfactor);
        res.SetOccupancy(atom.occupancy);
        OEChem::OEAtomSetResidue(oeatom, res);

        atom_ptrs.push_back(oeatom);
    }

    for (const auto& bond : maestro_mol.bonds) {
        if (bond.atom1_index < 0 ||
            static_cast<size_t>(bond.atom1_index) >= atom_ptrs.size() ||
            bond.atom2_index < 0 ||
            static_cast<size_t>(bond.atom2_index) >= atom_ptrs.size()) {
            throw MaestroConvertError(
                "Bond index out of range: atom1=" + std::to_string(bond.atom1_index) +
                " atom2=" + std::to_string(bond.atom2_index) +
                " num_atoms=" + std::to_string(atom_ptrs.size()));
        }
        mol.NewBond(
            atom_ptrs[static_cast<size_t>(bond.atom1_index)],
            atom_ptrs[static_cast<size_t>(bond.atom2_index)],
            static_cast<unsigned int>(bond.order));
    }

    mol.SetDimension(OEChem::OEGetDimensionFromCoords(mol));

    if (tags_ != TAG_NONE) {
        ApplyDataTags(maestro_mol, mol);
    }

    RunPerception(mol);
}

void MolConverter::SetTagFormat(OEMaestroTag tags) {
    tags_ = tags;
}

OEMaestroTag MolConverter::GetTagFormat() const {
    return tags_;
}

void MolConverter::SetPerception(OEMaestroPerception perception) {
    perception_ = perception;
}

OEMaestroPerception MolConverter::GetPerception() const {
    return perception_;
}

std::string MolConverter::FormatTagName(const std::string& key) const {
    // Maestro keys follow t_o_d format: type char, _, owner, _, data name
    // Example: r_m_pdb_tfactor -> type=r, owner=m, name=pdb_tfactor
    if (key.size() < 4 || key[1] != '_') {
        return key;
    }

    char type_char = key[0];
    size_t second_underscore = key.find('_', 2);
    if (second_underscore == std::string::npos) {
        return key;
    }

    std::string owner = key.substr(2, second_underscore - 2);
    std::string name = key.substr(second_underscore + 1);

    std::string result;
    if (tags_ & TAG_TYPE) {
        result += type_char;
    }
    if (tags_ & TAG_OWNER) {
        if (!result.empty()) result += '_';
        result += owner;
    }
    if (tags_ & TAG_NAME) {
        if (!result.empty()) result += '_';
        result += name;
    }
    return result;
}

void MolConverter::ApplyDataTags(const MaestroMol& maestro_mol,
                                  OEChem::OEMolBase& mol) const {
    // CT-level properties -> SD data
    for (const auto& [key, val] : maestro_mol.ct_properties) {
        OEChem::OESetSDData(mol, FormatTagName(key), val);
    }

    // Atom-level properties -> generic string data
    size_t i = 0;
    for (OESystem::OEIter<OEChem::OEAtomBase> ai = mol.GetAtoms(); ai; ++ai, ++i) {
        if (i < maestro_mol.atoms.size()) {
            for (const auto& [key, val] : maestro_mol.atoms[i].properties) {
                ai->SetStringData(FormatTagName(key).c_str(), val);
            }
        }
    }
}

void MolConverter::RunPerception(OEChem::OEMolBase& mol) const {
    if (perception_ & PERCEPTION_CONNECTIVITY) {
        if (OEChem::OEGetDimensionFromCoords(mol) == 3) {
            OEChem::OEDetermineConnectivity(mol);
        }
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

}  // namespace OEMaestro
