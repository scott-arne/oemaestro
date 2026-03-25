#ifndef OEMAESTRO_MAESTROMOL_H
#define OEMAESTRO_MAESTROMOL_H

#include <map>
#include <string>
#include <vector>

namespace OEMaestro {

/// Intermediate representation for a Maestro atom.
///
/// Stores atomic properties parsed from Maestro format files, including
/// coordinates, element type, PDB-style metadata, and custom properties.
struct MaestroAtom {
    int atomic_number = 0;        ///< Atomic number (e.g., 6 for carbon)
    double x = 0.0, y = 0.0, z = 0.0;  ///< Cartesian coordinates
    int formal_charge = 0;        ///< Formal charge on the atom
    std::string atom_name;        ///< PDB atom name
    std::string residue_name;     ///< PDB residue name
    int residue_number = 0;       ///< PDB residue number
    std::string chain_id;         ///< PDB chain identifier
    std::string insert_code;      ///< PDB insertion code
    double bfactor = 0.0;         ///< B-factor (temperature factor)
    double occupancy = 1.0;       ///< Occupancy
    int secondary_structure = -1; ///< Maestro secondary structure (0=loop, 1=helix, 2=strand, -1=unset)
    bool is_ligand_atom = false;  ///< True if Maestro i_psp_ligand_atom == 1
    std::map<std::string, std::string> properties;  ///< Additional atom-level properties
};

/// Intermediate representation for a Maestro bond.
///
/// Stores bond connectivity and order. Indices are 0-based.
struct MaestroBond {
    int atom1_index;  ///< 0-based index of first atom
    int atom2_index;  ///< 0-based index of second atom
    int order;        ///< Bond order (1=single, 2=double, 3=triple, etc.)
};

/// Intermediate representation for a Maestro molecule.
///
/// This is the primary IR used to hold parsed data from Maestro format files
/// before conversion to OpenEye OEMolBase objects. It stores raw data without
/// performing any chemical perception or validation.
struct MaestroMol {
    std::string title;                              ///< Molecule title
    std::vector<MaestroAtom> atoms;                 ///< Atom data
    std::vector<MaestroBond> bonds;                 ///< Bond data
    std::map<std::string, std::string> ct_properties;  ///< CT-level properties

    /// Returns the number of atoms in the molecule.
    ///
    /// :returns: Number of atoms.
    [[nodiscard]] size_t NumAtoms() const;

    /// Returns the number of bonds in the molecule.
    ///
    /// :returns: Number of bonds.
    [[nodiscard]] size_t NumBonds() const;

    /// Returns a string representation of the molecule.
    ///
    /// Useful for debugging and logging. Includes title, counts, and
    /// detailed information about atoms and bonds.
    ///
    /// :returns: String representation.
    [[nodiscard]] std::string ToString() const;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MAESTROMOL_H
