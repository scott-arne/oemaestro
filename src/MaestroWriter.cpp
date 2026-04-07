#include "oemaestro/MaestroWriter.h"
#include "oemaestro/Error.h"

#include <Writer.hpp>
#include <MaeBlock.hpp>
#include <MaeConstants.hpp>

#include <algorithm>
#include <fstream>
#include <map>

namespace mae = schrodinger::mae;

namespace OEMaestro {

namespace {

bool IsGzipFilename(const std::string& filename) {
    // Check for .mae.gz or .maegz extension
    if (filename.size() >= 7 && filename.substr(filename.size() - 7) == ".mae.gz")
        return true;
    if (filename.size() >= 6 && filename.substr(filename.size() - 6) == ".maegz")
        return true;
    return false;
}

/// Build a maeparser Block from a MaestroMol.
std::shared_ptr<mae::Block> BuildBlock(const MaestroMol& mol) {
    auto block = std::make_shared<mae::Block>(mae::CT_BLOCK);

    // Set title
    block->setStringProperty(mae::CT_TITLE, mol.title);

    // Set CT-level properties (type-aware based on t_o_d prefix)
    for (const auto& [key, val] : mol.ct_properties) {
        if (key.size() >= 4 && key[1] == '_') {
            char type_char = key[0];
            switch (type_char) {
                case 'i': {
                    try {
                        block->setIntProperty(key, std::stoi(val));
                    } catch (...) {
                        block->setStringProperty(key, val);
                    }
                    break;
                }
                case 'r': {
                    try {
                        block->setRealProperty(key, std::stod(val));
                    } catch (...) {
                        block->setStringProperty(key, val);
                    }
                    break;
                }
                case 'b': {
                    try {
                        block->setBoolProperty(key, std::stoi(val) != 0);
                    } catch (...) {
                        block->setStringProperty(key, val);
                    }
                    break;
                }
                default:
                    block->setStringProperty(key, val);
                    break;
            }
        } else {
            block->setStringProperty(key, val);
        }
    }

    auto ibm = std::make_shared<mae::IndexedBlockMap>();

    // Build atom indexed block
    if (!mol.atoms.empty()) {
        size_t n = mol.atoms.size();
        auto atom_block = std::make_shared<mae::IndexedBlock>(mae::ATOM_BLOCK);

        std::vector<int> atomic_nums(n);
        std::vector<double> x_coords(n), y_coords(n), z_coords(n);
        std::vector<int> formal_charges(n);
        std::vector<std::string> atom_names(n);
        std::vector<std::string> residue_names(n);
        std::vector<int> residue_numbers(n);
        std::vector<std::string> chain_names(n);
        std::vector<std::string> insertion_codes(n);
        std::vector<double> bfactors(n);
        std::vector<double> occupancies(n);
        std::vector<int> secondary_structures(n);
        std::vector<int> isotopes(n);
        std::vector<double> partial_charges(n);

        bool has_residue_info = false;
        bool has_isotope = false;
        bool has_partial_charge = false;
        bool has_secondary_structure = false;

        for (size_t i = 0; i < n; ++i) {
            const auto& a = mol.atoms[i];
            atomic_nums[i] = a.atomic_number;
            x_coords[i] = a.x;
            y_coords[i] = a.y;
            z_coords[i] = a.z;
            formal_charges[i] = a.formal_charge;
            atom_names[i] = a.atom_name;
            residue_names[i] = a.residue_name;
            residue_numbers[i] = a.residue_number;
            chain_names[i] = a.chain_id;
            insertion_codes[i] = a.insert_code;
            bfactors[i] = a.bfactor;
            occupancies[i] = a.occupancy;
            secondary_structures[i] = a.secondary_structure;
            isotopes[i] = a.isotope;
            partial_charges[i] = a.partial_charge;

            if (!a.residue_name.empty()) has_residue_info = true;
            if (a.isotope != 0) has_isotope = true;
            if (a.partial_charge != 0.0) has_partial_charge = true;
            if (a.secondary_structure >= 0) has_secondary_structure = true;
        }

        // Required properties
        atom_block->setIntProperty(mae::ATOM_ATOMIC_NUM,
            std::make_shared<mae::IndexedIntProperty>(atomic_nums));
        atom_block->setRealProperty(mae::ATOM_X_COORD,
            std::make_shared<mae::IndexedRealProperty>(x_coords));
        atom_block->setRealProperty(mae::ATOM_Y_COORD,
            std::make_shared<mae::IndexedRealProperty>(y_coords));
        atom_block->setRealProperty(mae::ATOM_Z_COORD,
            std::make_shared<mae::IndexedRealProperty>(z_coords));
        atom_block->setIntProperty(mae::ATOM_FORMAL_CHARGE,
            std::make_shared<mae::IndexedIntProperty>(formal_charges));
        atom_block->setStringProperty("s_m_pdb_atom_name",
            std::make_shared<mae::IndexedStringProperty>(atom_names));

        // Optional residue properties
        if (has_residue_info) {
            atom_block->setStringProperty("s_m_pdb_residue_name",
                std::make_shared<mae::IndexedStringProperty>(residue_names));
            atom_block->setIntProperty("i_m_residue_number",
                std::make_shared<mae::IndexedIntProperty>(residue_numbers));
            atom_block->setStringProperty("s_m_chain_name",
                std::make_shared<mae::IndexedStringProperty>(chain_names));
            atom_block->setStringProperty("s_m_pdb_insertion_code",
                std::make_shared<mae::IndexedStringProperty>(insertion_codes));
            atom_block->setRealProperty("r_m_pdb_tfactor",
                std::make_shared<mae::IndexedRealProperty>(bfactors));
            atom_block->setRealProperty("r_m_pdb_occupancy",
                std::make_shared<mae::IndexedRealProperty>(occupancies));
        }

        if (has_secondary_structure) {
            atom_block->setIntProperty("i_m_secondary_structure",
                std::make_shared<mae::IndexedIntProperty>(secondary_structures));
        }

        if (has_isotope) {
            atom_block->setIntProperty("i_m_isotope",
                std::make_shared<mae::IndexedIntProperty>(isotopes));
        }

        if (has_partial_charge) {
            atom_block->setRealProperty("r_m_charge1",
                std::make_shared<mae::IndexedRealProperty>(partial_charges));
        }

        // Non-structural atom properties
        // Collect all unique property keys across atoms
        std::map<std::string, bool> prop_keys;
        for (const auto& a : mol.atoms) {
            for (const auto& [k, v] : a.properties) {
                prop_keys[k] = true;
            }
        }

        for (const auto& [key, _] : prop_keys) {
            char type_char = (key.size() >= 4 && key[1] == '_') ? key[0] : 's';
            switch (type_char) {
                case 'i': case 'b': {
                    std::vector<int> vals(n, 0);
                    for (size_t i = 0; i < n; ++i) {
                        auto it = mol.atoms[i].properties.find(key);
                        if (it != mol.atoms[i].properties.end()) {
                            try { vals[i] = std::stoi(it->second); } catch (...) {}
                        }
                    }
                    atom_block->setIntProperty(key,
                        std::make_shared<mae::IndexedIntProperty>(vals));
                    break;
                }
                case 'r': {
                    std::vector<double> vals(n, 0.0);
                    for (size_t i = 0; i < n; ++i) {
                        auto it = mol.atoms[i].properties.find(key);
                        if (it != mol.atoms[i].properties.end()) {
                            try { vals[i] = std::stod(it->second); } catch (...) {}
                        }
                    }
                    atom_block->setRealProperty(key,
                        std::make_shared<mae::IndexedRealProperty>(vals));
                    break;
                }
                default: {
                    std::vector<std::string> vals(n);
                    for (size_t i = 0; i < n; ++i) {
                        auto it = mol.atoms[i].properties.find(key);
                        if (it != mol.atoms[i].properties.end())
                            vals[i] = it->second;
                    }
                    atom_block->setStringProperty(key,
                        std::make_shared<mae::IndexedStringProperty>(vals));
                    break;
                }
            }
        }

        ibm->addIndexedBlock(mae::ATOM_BLOCK, atom_block);
    }

    // Build bond indexed block
    if (!mol.bonds.empty()) {
        size_t nb = mol.bonds.size();
        auto bond_block = std::make_shared<mae::IndexedBlock>(mae::BOND_BLOCK);

        std::vector<int> from_indices(nb), to_indices(nb), orders(nb);
        for (size_t i = 0; i < nb; ++i) {
            // Maestro uses 1-based indices
            from_indices[i] = mol.bonds[i].atom1_index + 1;
            to_indices[i] = mol.bonds[i].atom2_index + 1;
            orders[i] = mol.bonds[i].order;
        }

        bond_block->setIntProperty(mae::BOND_ATOM_1,
            std::make_shared<mae::IndexedIntProperty>(from_indices));
        bond_block->setIntProperty(mae::BOND_ATOM_2,
            std::make_shared<mae::IndexedIntProperty>(to_indices));
        bond_block->setIntProperty(mae::BOND_ORDER,
            std::make_shared<mae::IndexedIntProperty>(orders));

        ibm->addIndexedBlock(mae::BOND_BLOCK, bond_block);
    }

    block->setIndexedBlockMap(ibm);
    return block;
}

}  // anonymous namespace

struct MaestroWriter::Impl {
    std::unique_ptr<mae::Writer> writer;   // Only for CREATE mode
    std::shared_ptr<std::ostream> stream;  // For APPEND mode
    bool closed = false;
    OEMaestroWriteMode mode;

    Impl(const std::string& filename, OEMaestroWriteMode m) : mode(m) {
        if (m == WRITE_APPEND) {
            if (IsGzipFilename(filename)) {
                throw OEMaestroError("APPEND mode is not supported for compressed files");
            }
            stream = std::make_shared<std::ofstream>(
                filename, std::ios::out | std::ios::app);
            if (!stream || !(*stream)) {
                throw MaestroParseError("Failed to open file for append: " + filename);
            }
        } else {
            try {
                writer = std::make_unique<mae::Writer>(filename);
            } catch (const std::exception& e) {
                throw MaestroParseError("Failed to open file for writing: " + filename + ": " + e.what());
            }
        }
    }

    Impl(std::shared_ptr<std::ostream> s, OEMaestroWriteMode m) : mode(m) {
        if (m == WRITE_CREATE) {
            try {
                writer = std::make_unique<mae::Writer>(s);
            } catch (const std::exception& e) {
                throw MaestroParseError(
                    std::string("Failed to create writer from stream: ") + e.what());
            }
        } else {
            stream = std::move(s);
        }
    }

    bool Write(const MaestroMol& mol) {  // NOLINT(readability-make-member-function-const) -- mutates writer state
        if (closed) {
            throw OEMaestroError("Cannot write to closed MaestroWriter");
        }

        auto block = BuildBlock(mol);

        if (writer) {
            writer->write(block);
        } else if (stream) {
            block->write(*stream);
        }

        if (stream && stream->fail()) return false;
        return true;
    }

    void Close() {
        if (closed) return;
        writer.reset();
        if (stream) {
            stream->flush();
            stream.reset();
        }
        closed = true;
    }
};

MaestroWriter::MaestroWriter(const std::string& filename, OEMaestroWriteMode mode)
    : impl_(std::make_unique<Impl>(filename, mode)) {}

MaestroWriter::MaestroWriter(std::shared_ptr<std::ostream> stream, OEMaestroWriteMode mode)
    : impl_(std::make_unique<Impl>(std::move(stream), mode)) {}

MaestroWriter::~MaestroWriter() = default;
MaestroWriter::MaestroWriter(MaestroWriter&&) noexcept = default;
MaestroWriter& MaestroWriter::operator=(MaestroWriter&&) noexcept = default;

bool MaestroWriter::Write(const MaestroMol& mol) { return impl_->Write(mol); }
void MaestroWriter::Close() { impl_->Close(); }

}  // namespace OEMaestro
