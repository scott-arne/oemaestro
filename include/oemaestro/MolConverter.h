#ifndef OEMAESTRO_MOLCONVERTER_H
#define OEMAESTRO_MOLCONVERTER_H

#include "oemaestro/Enums.h"
#include "oemaestro/MaestroMol.h"

#include <oechem.h>
#include <string>

namespace OEMaestro {

/// Converts MaestroMol intermediate representation to OpenEye OEMolBase.
///
/// Handles atom/bond creation, coordinate assignment, residue info mapping,
/// data tag formatting (Maestro ``t_o_d`` keys), and chemical perception.
class MolConverter {
public:
    /// Construct with default settings (TAG_ALL, PERCEPTION_ALL).
    MolConverter();

    /// Construct with explicit tag format and perception settings.
    ///
    /// :param tags: Bitmask controlling data tag name formatting.
    /// :param perception: Bitmask controlling post-conversion perception steps.
    explicit MolConverter(OEMaestroTag tags, OEMaestroPerception perception = PERCEPTION_ALL);

    /// Convert a MaestroMol into the provided OEMolBase.
    ///
    /// Clears the target molecule first, then populates atoms, bonds,
    /// coordinates, residue info, data tags, and runs perception.
    ///
    /// :param maestro_mol: Source intermediate representation.
    /// :param mol: Target OpenEye molecule (cleared before use).
    /// :raises MaestroConvertError: If bond indices are out of range.
    void Convert(const MaestroMol& maestro_mol, OEChem::OEMolBase& mol) const;

    /// Set the tag format bitmask.
    ///
    /// :param tags: New tag format bitmask.
    void SetTagFormat(OEMaestroTag tags);

    /// Get the current tag format bitmask.
    ///
    /// :returns: Current tag format bitmask.
    OEMaestroTag GetTagFormat() const;

    /// Set the perception bitmask.
    ///
    /// :param perception: New perception bitmask.
    void SetPerception(OEMaestroPerception perception);

    /// Get the current perception bitmask.
    ///
    /// :returns: Current perception bitmask.
    OEMaestroPerception GetPerception() const;

private:
    /// Format a Maestro ``t_o_d`` key according to the current tag bitmask.
    ///
    /// :param key: Raw Maestro property key (e.g., ``r_m_pdb_tfactor``).
    /// :returns: Formatted tag name.
    std::string FormatTagName(const std::string& key) const;

    /// Apply CT-level and atom-level data tags to the molecule.
    ///
    /// :param maestro_mol: Source intermediate representation.
    /// :param mol: Target molecule to receive data tags.
    void ApplyDataTags(const MaestroMol& maestro_mol, OEChem::OEMolBase& mol) const;

    /// Run perception steps according to the current perception bitmask.
    ///
    /// :param mol: Molecule to perceive.
    void RunPerception(OEChem::OEMolBase& mol) const;

    OEMaestroTag tags_;
    OEMaestroPerception perception_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MOLCONVERTER_H
