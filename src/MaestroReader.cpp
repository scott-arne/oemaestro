#include "oemaestro/MaestroReader.h"
#include "oemaestro/Error.h"

#include <Reader.hpp>
#include <MaeBlock.hpp>
#include <MaeConstants.hpp>

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace OEMaestro {

// Structural property keys that get mapped to MaestroAtom fields (not stored in properties map)
static const std::unordered_set<std::string> STRUCTURAL_ATOM_PROPS = {
    "i_m_atomic_number", "r_m_x_coord", "r_m_y_coord", "r_m_z_coord",
    "i_m_formal_charge", "s_m_pdb_atom_name", "s_m_pdb_residue_name",
    "i_m_residue_number", "s_m_chain_name", "s_m_pdb_insertion_code",
    "r_m_pdb_tfactor", "r_m_pdb_occupancy", "i_m_secondary_structure",
    "i_psp_ligand_atom"
};

// CT-level structural properties to skip in data tags
static const std::unordered_set<std::string> STRUCTURAL_CT_PROPS = {
    "s_m_title", "s_m_entry_name", "s_m_entry_id"
};

struct MaestroReader::Impl {
    std::unique_ptr<schrodinger::mae::Reader> reader;

    explicit Impl(const std::string& filename) {
        try {
            reader = std::make_unique<schrodinger::mae::Reader>(filename);
        } catch (const std::exception& e) {
            throw MaestroParseError("Failed to open Maestro file '" +
                                    filename + "': " + e.what());
        }
    }

    explicit Impl(std::shared_ptr<std::istream> stream) {
        try {
            reader = std::make_unique<schrodinger::mae::Reader>(stream);
        } catch (const std::exception& e) {
            throw MaestroParseError(
                std::string("Failed to create Maestro reader from stream: ") +
                e.what());
        }
    }

    bool Read(MaestroMol& mol) {
        std::shared_ptr<schrodinger::mae::Block> block;
        try {
            block = reader->next(schrodinger::mae::CT_BLOCK);
        } catch (const std::exception& e) {
            throw MaestroParseError(std::string("Error reading CT block: ") +
                                    e.what());
        }

        if (!block) return false;

        mol.Clear();

        // Extract title
        if (block->hasStringProperty(schrodinger::mae::CT_TITLE)) {
            mol.title = block->getStringProperty(schrodinger::mae::CT_TITLE);
        } else if (block->hasStringProperty("s_m_entry_name")) {
            mol.title = block->getStringProperty("s_m_entry_name");
        }

        // Extract CT-level properties (non-structural ones go to ct_properties)
        for (const auto& [key, val] : block->getProperties<std::string>()) {
            if (STRUCTURAL_CT_PROPS.count(key) == 0) {
                mol.ct_properties[key] = val;
            }
        }
        for (const auto& [key, val] : block->getProperties<int>()) {
            if (STRUCTURAL_CT_PROPS.count(key) == 0) {
                mol.ct_properties[key] = std::to_string(val);
            }
        }
        for (const auto& [key, val] : block->getProperties<double>()) {
            if (STRUCTURAL_CT_PROPS.count(key) == 0) {
                mol.ct_properties[key] = std::to_string(val);
            }
        }
        for (const auto& [key, val] : block->getProperties<schrodinger::mae::BoolProperty>()) {
            if (STRUCTURAL_CT_PROPS.count(key) == 0) {
                mol.ct_properties[key] = (val == 1u) ? "1" : "0";
            }
        }

        // Extract atoms
        if (block->hasIndexedBlock(schrodinger::mae::ATOM_BLOCK)) {
            auto atom_block =
                block->getIndexedBlock(schrodinger::mae::ATOM_BLOCK);
            size_t num_atoms = atom_block->size();
            mol.atoms.resize(num_atoms);

            // Get structural property columns
            auto z_prop =
                atom_block->getIntProperty(schrodinger::mae::ATOM_ATOMIC_NUM);
            auto x_prop =
                atom_block->getRealProperty(schrodinger::mae::ATOM_X_COORD);
            auto y_prop =
                atom_block->getRealProperty(schrodinger::mae::ATOM_Y_COORD);
            auto z_coord =
                atom_block->getRealProperty(schrodinger::mae::ATOM_Z_COORD);
            auto fc_prop =
                atom_block->getIntProperty(schrodinger::mae::ATOM_FORMAL_CHARGE);
            auto aname_prop =
                atom_block->getStringProperty("s_m_pdb_atom_name");
            auto rname_prop =
                atom_block->getStringProperty("s_m_pdb_residue_name");
            auto rnum_prop =
                atom_block->getIntProperty("i_m_residue_number");
            auto chain_prop =
                atom_block->getStringProperty("s_m_chain_name");
            auto icode_prop =
                atom_block->getStringProperty("s_m_pdb_insertion_code");
            auto bfac_prop =
                atom_block->getRealProperty("r_m_pdb_tfactor");
            auto occ_prop =
                atom_block->getRealProperty("r_m_pdb_occupancy");
            auto ss_prop =
                atom_block->getIntProperty("i_m_secondary_structure");
            auto ligand_prop =
                atom_block->getIntProperty("i_psp_ligand_atom");

            for (size_t i = 0; i < num_atoms; ++i) {
                auto& atom = mol.atoms[i];
                if (z_prop && z_prop->isDefined(i))
                    atom.atomic_number = (*z_prop)[i];
                if (x_prop && x_prop->isDefined(i))
                    atom.x = (*x_prop)[i];
                if (y_prop && y_prop->isDefined(i))
                    atom.y = (*y_prop)[i];
                if (z_coord && z_coord->isDefined(i))
                    atom.z = (*z_coord)[i];
                if (fc_prop && fc_prop->isDefined(i))
                    atom.formal_charge = (*fc_prop)[i];
                if (aname_prop && aname_prop->isDefined(i))
                    atom.atom_name = (*aname_prop)[i];
                if (rname_prop && rname_prop->isDefined(i))
                    atom.residue_name = (*rname_prop)[i];
                if (rnum_prop && rnum_prop->isDefined(i))
                    atom.residue_number = (*rnum_prop)[i];
                if (chain_prop && chain_prop->isDefined(i))
                    atom.chain_id = (*chain_prop)[i];
                if (icode_prop && icode_prop->isDefined(i))
                    atom.insert_code = (*icode_prop)[i];
                if (bfac_prop && bfac_prop->isDefined(i))
                    atom.bfactor = (*bfac_prop)[i];
                if (occ_prop && occ_prop->isDefined(i))
                    atom.occupancy = (*occ_prop)[i];
                if (ss_prop && ss_prop->isDefined(i))
                    atom.secondary_structure = (*ss_prop)[i];
                if (ligand_prop && ligand_prop->isDefined(i))
                    atom.is_ligand_atom = ((*ligand_prop)[i] == 1);
            }

            // Extract non-structural atom properties
            for (const auto& [key, prop] :
                 atom_block->getProperties<int>()) {
                if (STRUCTURAL_ATOM_PROPS.count(key)) continue;
                for (size_t i = 0; i < num_atoms; ++i) {
                    if (prop->isDefined(i)) {
                        mol.atoms[i].properties[key] =
                            std::to_string((*prop)[i]);
                    }
                }
            }
            for (const auto& [key, prop] :
                 atom_block->getProperties<double>()) {
                if (STRUCTURAL_ATOM_PROPS.count(key)) continue;
                for (size_t i = 0; i < num_atoms; ++i) {
                    if (prop->isDefined(i)) {
                        mol.atoms[i].properties[key] =
                            std::to_string((*prop)[i]);
                    }
                }
            }
            for (const auto& [key, prop] :
                 atom_block->getProperties<std::string>()) {
                if (STRUCTURAL_ATOM_PROPS.count(key)) continue;
                for (size_t i = 0; i < num_atoms; ++i) {
                    if (prop->isDefined(i)) {
                        mol.atoms[i].properties[key] = (*prop)[i];
                    }
                }
            }
        }

        // Extract bonds
        if (block->hasIndexedBlock(schrodinger::mae::BOND_BLOCK)) {
            auto bond_block =
                block->getIndexedBlock(schrodinger::mae::BOND_BLOCK);
            size_t num_bonds = bond_block->size();
            mol.bonds.resize(num_bonds);

            auto from_prop =
                bond_block->getIntProperty(schrodinger::mae::BOND_ATOM_1);
            auto to_prop =
                bond_block->getIntProperty(schrodinger::mae::BOND_ATOM_2);
            auto order_prop =
                bond_block->getIntProperty(schrodinger::mae::BOND_ORDER);

            if (!from_prop || !to_prop || !order_prop) {
                throw MaestroParseError(
                    "Bond block missing required property column(s): " +
                    std::string(!from_prop ? "bond_atom_1 " : "") +
                    std::string(!to_prop ? "bond_atom_2 " : "") +
                    std::string(!order_prop ? "bond_order" : ""));
            }

            for (size_t i = 0; i < num_bonds; ++i) {
                auto& bond = mol.bonds[i];
                // Convert from 1-based to 0-based
                bond.atom1_index = (*from_prop)[i] - 1;
                bond.atom2_index = (*to_prop)[i] - 1;
                bond.order = (*order_prop)[i];
            }
        }

        return true;
    }
};

MaestroReader::MaestroReader(const std::string& filename)
    : pimpl_(std::make_unique<Impl>(filename)) {}

MaestroReader::MaestroReader(std::shared_ptr<std::istream> stream)
    : pimpl_(std::make_unique<Impl>(std::move(stream))) {}

bool MaestroReader::Read(MaestroMol& mol) { return pimpl_->Read(mol); }

MaestroReader::~MaestroReader() = default;
MaestroReader::MaestroReader(MaestroReader&&) noexcept = default;
MaestroReader& MaestroReader::operator=(MaestroReader&&) noexcept = default;

}  // namespace OEMaestro
