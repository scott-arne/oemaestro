#ifndef OEMAESTRO_ENUMS_H
#define OEMAESTRO_ENUMS_H

#include <algorithm>
#include <string>
#include <thread>

namespace OEMaestro {

/// Bitmask controlling data tag name formatting from Maestro t_o_d keys.
enum OEMaestroTag : unsigned int {
    TAG_NONE  = 0x0,
    TAG_TYPE  = 0x1,
    TAG_OWNER = 0x2,
    TAG_NAME  = 0x4,
    TAG_ALL   = TAG_TYPE | TAG_OWNER | TAG_NAME
};

inline OEMaestroTag operator|(OEMaestroTag a, OEMaestroTag b) {
    return static_cast<OEMaestroTag>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
inline OEMaestroTag operator&(OEMaestroTag a, OEMaestroTag b) {
    return static_cast<OEMaestroTag>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
inline OEMaestroTag operator^(OEMaestroTag a, OEMaestroTag b) {
    return static_cast<OEMaestroTag>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
}
inline OEMaestroTag operator~(OEMaestroTag a) {
    return static_cast<OEMaestroTag>(~static_cast<unsigned>(a));
}

/// Bitmask controlling post-parse perception steps.
/// Steps execute in the listed order; steps not in the mask are skipped.
enum OEMaestroPerception : unsigned int {
    PERCEPTION_NONE               = 0x0,
    PERCEPTION_CONNECTIVITY       = 0x1,
    PERCEPTION_RINGS              = 0x2,
    PERCEPTION_BOND_ORDERS        = 0x4,
    PERCEPTION_IMPLICIT_HYDROGENS = 0x8,
    PERCEPTION_FORMAL_CHARGES     = 0x10,
    PERCEPTION_ALL                = PERCEPTION_CONNECTIVITY | PERCEPTION_RINGS
                                  | PERCEPTION_BOND_ORDERS | PERCEPTION_IMPLICIT_HYDROGENS
                                  | PERCEPTION_FORMAL_CHARGES,
    PERCEPTION_DEFAULT            = PERCEPTION_RINGS | PERCEPTION_IMPLICIT_HYDROGENS
                                  | PERCEPTION_FORMAL_CHARGES
};

inline OEMaestroPerception operator|(OEMaestroPerception a, OEMaestroPerception b) {
    return static_cast<OEMaestroPerception>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
inline OEMaestroPerception operator&(OEMaestroPerception a, OEMaestroPerception b) {
    return static_cast<OEMaestroPerception>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
inline OEMaestroPerception operator^(OEMaestroPerception a, OEMaestroPerception b) {
    return static_cast<OEMaestroPerception>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
}
inline OEMaestroPerception operator~(OEMaestroPerception a) {
    return static_cast<OEMaestroPerception>(~static_cast<unsigned>(a));
}

/// Return min(2, hardware threads), falling back to 1.
inline unsigned int default_num_threads() {
    unsigned int hw = std::thread::hardware_concurrency();
    return (hw == 0) ? 1 : std::min(hw, 2u);
}

/// Reader configuration combining tag format and perception.
class OEMaestroReaderConfig {
public:
    OEMaestroReaderConfig() = default;

    OEMaestroReaderConfig(OEMaestroTag tags, OEMaestroPerception perception,
                          unsigned int num_threads = default_num_threads())
        : tags_(tags), perception_(perception), num_threads_(num_threads) {}

    void SetTags(OEMaestroTag tags) { tags_ = tags; }
    OEMaestroTag GetTags() const { return tags_; }

    void SetPerception(OEMaestroPerception perception) { perception_ = perception; }
    OEMaestroPerception GetPerception() const { return perception_; }

    void SetNumThreads(unsigned int num_threads) { num_threads_ = num_threads; }
    unsigned int GetNumThreads() const { return num_threads_; }

private:
    OEMaestroTag tags_ = TAG_ALL;
    OEMaestroPerception perception_ = PERCEPTION_DEFAULT;
    unsigned int num_threads_ = default_num_threads();
};

/// Write mode for MaestroWriter.
enum OEMaestroWriteMode : unsigned int {
    WRITE_CREATE = 0,   ///< Overwrite/create new file (writes header block)
    WRITE_APPEND = 1    ///< Append CT blocks to existing file (no header)
};

/// Writer configuration combining tag format, write mode, and default owner.
class OEMaestroWriterConfig {
public:
    OEMaestroWriterConfig() = default;

    OEMaestroWriterConfig(OEMaestroTag tags, OEMaestroWriteMode mode,
                          const std::string& default_owner = "user")
        : tags_(tags), mode_(mode), default_owner_(default_owner) {}

    void SetTags(OEMaestroTag tags) { tags_ = tags; }
    OEMaestroTag GetTags() const { return tags_; }

    void SetMode(OEMaestroWriteMode mode) { mode_ = mode; }
    OEMaestroWriteMode GetMode() const { return mode_; }

    void SetDefaultOwner(const std::string& owner) { default_owner_ = owner; }
    const std::string& GetDefaultOwner() const { return default_owner_; }

private:
    OEMaestroTag tags_ = TAG_ALL;
    OEMaestroWriteMode mode_ = WRITE_CREATE;
    std::string default_owner_ = "user";
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_ENUMS_H
