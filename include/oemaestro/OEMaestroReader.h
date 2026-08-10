#ifndef OEMAESTRO_OEMAESTROREADER_H
#define OEMAESTRO_OEMAESTROREADER_H

#include <memory>
#include <string>

#include <oechem.h>

#include "oemaestro/Enums.h"
#include "oemaestro/ReadStatus.h"

namespace OEMaestro {

/// High-level reader that produces OpenEye OEMol molecules from Maestro files.
///
/// Supports conformer grouping via SetConfTest. With the default conf test
/// (OEDefaultConfTest), each CT block becomes a separate OEMol. With a grouping
/// conf test (e.g. OEIsomericConfTest), consecutive matching CTs are grouped as
/// conformers in a single OEMol.
///
/// Example::
///
///     OEMaestroReader reader("input.mae");
///     OEChem::OEMol mol;
///     while (reader.Read(mol)) {
///         // process mol
///     }
class OEMaestroReader {
public:
    /// Constructs a reader from a filename.
    ///
    /// :param filename: Path to a Maestro file (.mae or .mae.gz).
    /// :param config: Reader configuration for tag format and perception.
    /// :raises MaestroParseError: If the file cannot be opened.
    explicit OEMaestroReader(const std::string& filename,
                             OEMaestroReaderConfig config = {});

    /// Constructs a reader from an OpenEye oeifstream.
    ///
    /// :param ifs: An open OpenEye input file stream.
    /// :param config: Reader configuration for tag format and perception.
    /// :raises MaestroParseError: If the stream is invalid.
    explicit OEMaestroReader(OEPlatform::oeifstream& ifs,
                             OEMaestroReaderConfig config = {});

    /// Reads the next molecule with conformer grouping support.
    ///
    /// With default conf test, each CT block becomes a separate OEMol.
    /// With a grouping conf test (e.g. OEIsomericConfTest), consecutive
    /// matching CTs are grouped as conformers in a single OEMol.
    ///
    /// :param mol: OEMol to populate.
    /// :returns: True if a molecule was read, false at EOF.
    bool Read(OEChem::OEMol& mol);

    /// Reads the next CT block as a single molecule (no conformer grouping).
    ///
    /// This overload is suitable when conformer grouping is not needed.
    /// Each call reads one CT block regardless of the conf test setting.
    ///
    /// :param mol: OEMolBase to populate.
    /// :returns: True if a molecule was read, false at EOF.
    bool Read(OEChem::OEMolBase& mol);

    /// Reads the next molecule without throwing on a malformed CT block.
    ///
    /// Read() throws MaestroParseError on a bad block, which forces every caller
    /// that wants to report the failure to wrap the call. TryRead reports the
    /// same information as a value.
    ///
    /// Failures that are not std::exception propagate; TryRead converts parse
    /// and I/O errors, not foreign throws.
    ///
    /// :param mol: OEMol to populate. Untouched unless the status is Ok.
    /// :returns: Ok, EndOfStream, or RecordError with a diagnostic.
    ReadResult TryRead(OEChem::OEMol& mol);

    /// Non-conformer-grouping overload, matching Read(OEChem::OEMolBase&).
    ///
    /// Failures that are not std::exception propagate; TryRead converts parse
    /// and I/O errors, not foreign throws.
    ///
    /// :param mol: OEMolBase to populate. Untouched unless the status is Ok.
    /// :returns: Ok, EndOfStream, or RecordError with a diagnostic.
    ReadResult TryRead(OEChem::OEMolBase& mol);

    /// :returns: False, always. maeparser cannot resume after a failed CT block.
    [[nodiscard]] bool CanResynchronize() const;

    /// Sets the conformer test for grouping CT blocks.
    ///
    /// Takes ownership of conf_test. Passing nullptr resets to OEDefaultConfTest.
    ///
    /// :param conf_test: Conformer test object (ownership transferred).
    void SetConfTest(OEChem::OEConfTestBase* conf_test);

    /// Gets the current perception bitmask.
    ///
    /// :returns: Current perception bitmask.
    OEMaestroPerception GetPerception() const;  // NOLINT(modernize-use-nodiscard)

    /// Gets the current tag format bitmask.
    ///
    /// :returns: Current tag format bitmask.
    OEMaestroTag GetTagFormat() const;          // NOLINT(modernize-use-nodiscard)

    /// Gets the current reader configuration.
    ///
    /// :returns: Current reader configuration.
    OEMaestroReaderConfig GetConfig() const;    // NOLINT(modernize-use-nodiscard)

    ~OEMaestroReader();
    OEMaestroReader(const OEMaestroReader&) = delete;
    OEMaestroReader& operator=(const OEMaestroReader&) = delete;
    OEMaestroReader(OEMaestroReader&&) noexcept;
    OEMaestroReader& operator=(OEMaestroReader&&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEMAESTROREADER_H
