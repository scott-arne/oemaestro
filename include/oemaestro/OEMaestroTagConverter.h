#ifndef OEMAESTRO_OEMAESTROTAGCONVERTER_H
#define OEMAESTRO_OEMAESTROTAGCONVERTER_H

#include <string>
#include <vector>
#include "oemaestro/Enums.h"

namespace OEMaestro {

struct MaestroMol;

/// Bidirectional data tag key conversion utility.
///
/// Handles conversion between full Maestro t_o_d keys (e.g., ``r_m_x_coord``)
/// and formatted keys controlled by OEMaestroTag bitmask. Also provides bulk
/// conversion between MaestroMol IR and OEMolBase generic data.
class OEMaestroTagConverter {
public:
    /// Construct with tag format bitmask and default owner for inference.
    ///
    /// :param tags: Bitmask controlling which key components to include.
    /// :param default_owner: Owner used when reconstructing keys missing an owner.
    explicit OEMaestroTagConverter(OEMaestroTag tags = TAG_ALL,
                                   const std::string& default_owner = "user");

    /// Convert a full Maestro key (t_o_d) to a formatted key per the tag bitmask.
    ///
    /// :param maestro_key: Full Maestro key (e.g., ``r_m_x_coord``).
    /// :returns: Formatted key. Non-t_o_d keys returned as-is.
    std::string ToFormatted(const std::string& maestro_key) const;

    /// Convert a formatted key back to a full Maestro t_o_d key.
    ///
    /// If the key is already in full t_o_d format, returns as-is.
    /// If the type prefix is missing, uses type_hint (defaults to 's').
    /// If the owner is missing, uses the default_owner.
    ///
    /// :param formatted_key: The formatted key to reconstruct.
    /// :param type_hint: OE data type char ('i','r','s','b') for inference.
    /// :returns: Full Maestro t_o_d key.
    std::string ToMaestroTag(const std::string& formatted_key,
                             char type_hint = '\0') const;

    /// Check if a key matches the full Maestro t_o_d format.
    ///
    /// :param key: The key to check.
    /// :returns: True if the key has a valid type prefix, underscore, owner, underscore, name.
    static bool IsFullMaestroKey(const std::string& key);

    OEMaestroTag GetTags() const;
    void SetTags(OEMaestroTag tags);
    const std::string& GetDefaultOwner() const;
    void SetDefaultOwner(const std::string& owner);

private:
    OEMaestroTag tags_;
    std::string default_owner_;

    /// Check if a character is a valid Maestro type prefix.
    static bool IsValidTypePrefix(char c);
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEMAESTROTAGCONVERTER_H
