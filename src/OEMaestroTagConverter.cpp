#include "oemaestro/OEMaestroTagConverter.h"

namespace OEMaestro {

OEMaestroTagConverter::OEMaestroTagConverter(OEMaestroTag tags,  // NOLINT(performance-unnecessary-value-param)
                                             const std::string& default_owner)
    : tags_(tags), default_owner_(default_owner) {}

bool OEMaestroTagConverter::IsValidTypePrefix(char c) {
    return c == 'i' || c == 'r' || c == 's' || c == 'b';
}

bool OEMaestroTagConverter::IsFullMaestroKey(const std::string& key) {
    // Minimum: "t_o_n" = 5 chars
    if (key.size() < 5 || key[1] != '_') return false;
    if (!IsValidTypePrefix(key[0])) return false;
    // Must have owner_name after type_
    auto second_underscore = key.find('_', 2);
    if (second_underscore == std::string::npos) return false;
    // Owner must be non-empty
    if (second_underscore == 2) return false;
    // Name must be non-empty
    if (second_underscore + 1 >= key.size()) return false;
    return true;
}

std::string OEMaestroTagConverter::ToFormatted(const std::string& maestro_key) const {
    if (!IsFullMaestroKey(maestro_key)) return maestro_key;
    if (tags_ == TAG_ALL) return maestro_key;

    char type_char = maestro_key[0];
    auto second_underscore = maestro_key.find('_', 2);
    std::string owner = maestro_key.substr(2, second_underscore - 2);
    std::string name = maestro_key.substr(second_underscore + 1);

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

std::string OEMaestroTagConverter::ToMaestroTag(const std::string& formatted_key,
                                                 char type_hint) const {
    if (IsFullMaestroKey(formatted_key)) return formatted_key;

    char type_char = (type_hint != '\0') ? type_hint : 's';

    // Check if the key starts with a type prefix
    if (formatted_key.size() >= 2 && IsValidTypePrefix(formatted_key[0])
        && formatted_key[1] == '_') {
        // Key starts with "t_..." pattern
        type_char = formatted_key[0];
        std::string rest = formatted_key.substr(2);

        // Check if rest contains owner_name pattern (second underscore)
        auto second_underscore = rest.find('_');
        if (second_underscore != std::string::npos && second_underscore > 0
            && second_underscore + 1 < rest.size()) {
            // "t_owner_name" pattern - type prefix present, owner_name follows
            return std::string(1, type_char) + "_" + rest;
        } else {  // NOLINT(readability-else-after-return)
            // "t_name" pattern - type prefix present, but no owner in rest
            // Need to insert default owner
            return std::string(1, type_char) + "_" + default_owner_ + "_" + rest;
        }
    }

    // No type prefix - check if there's an underscore suggesting owner_name
    auto first_underscore = formatted_key.find('_');
    if (first_underscore != std::string::npos && first_underscore > 0
        && first_underscore + 1 < formatted_key.size()) {
        // Contains underscore - could be "owner_name" or "name_with_underscore"
        // We treat single-char first segment as likely owner, otherwise as name
        if (first_underscore == 1) {
            // Single char before underscore - likely owner (e.g., "m_pdb_tfactor")
            return std::string(1, type_char) + "_" + formatted_key;
        }
    }

    // Plain name (no underscores or multi-char first segment)
    return std::string(1, type_char) + "_" + default_owner_ + "_" + formatted_key;
}

OEMaestroTag OEMaestroTagConverter::GetTags() const { return tags_; }
void OEMaestroTagConverter::SetTags(OEMaestroTag tags) { tags_ = tags; }
const std::string& OEMaestroTagConverter::GetDefaultOwner() const { return default_owner_; }
void OEMaestroTagConverter::SetDefaultOwner(const std::string& owner) { default_owner_ = owner; }

}  // namespace OEMaestro
