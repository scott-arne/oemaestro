#include "oemaestro/MaestroMol.h"
#include <sstream>

namespace OEMaestro {

size_t MaestroMol::NumAtoms() const { return atoms.size(); }

size_t MaestroMol::NumBonds() const { return bonds.size(); }

std::string MaestroMol::ToString() const {
    std::ostringstream oss;
    oss << "MaestroMol(title=\"" << title
        << "\", atoms=" << atoms.size()
        << ", bonds=" << bonds.size()
        << ", ct_properties=" << ct_properties.size() << ")";
    for (size_t i = 0; i < atoms.size(); ++i) {
        const auto& a = atoms[i];
        oss << "\n  Atom " << i << ": Z=" << a.atomic_number
            << " (" << a.x << ", " << a.y << ", " << a.z << ")"
            << " name=\"" << a.atom_name << "\""
            << " res=" << a.residue_name << ":" << a.residue_number
            << ":" << a.chain_id;
        if (!a.properties.empty())
            oss << " [" << a.properties.size() << " props]";
    }
    for (size_t i = 0; i < bonds.size(); ++i) {
        const auto& b = bonds[i];
        oss << "\n  Bond " << i << ": " << b.atom1_index
            << "-" << b.atom2_index << " order=" << b.order;
    }
    return oss.str();
}

}  // namespace OEMaestro
