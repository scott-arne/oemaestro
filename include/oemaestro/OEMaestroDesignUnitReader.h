#ifndef OEMAESTRO_OEMAESTRODESIGNUNITREADER_H
#define OEMAESTRO_OEMAESTRODESIGNUNITREADER_H

#include <memory>
#include <string>

#include <oechem.h>
#include <oebio.h>

#include "oemaestro/Enums.h"

namespace OEMaestro {

/// High-level reader that produces OpenEye OEDesignUnit objects from Maestro files.
///
/// Each CT block is read as a full molecule, then split into protein, ligand,
/// solvent, and cofactor components using atom predicates. Default classification:
///
/// - **Ligand**: atoms with ``is_ligand_atom`` BoolData (from ``i_psp_ligand_atom``)
/// - **Solvent**: residue name in PDB solvent code set (excludes metals/ions)
/// - **Cofactor**: residue name in PDB cofactor code set (excludes solvent-priority codes)
/// - **Protein**: everything else
///
/// Custom predicates can override any component classification.
///
/// Example::
///
///     OEMaestroDesignUnitReader reader("prepared.mae");
///     OEBio::OEDesignUnit du;
///     while (reader.Read(du)) {
///         // process du
///     }
class OEMaestroDesignUnitReader {
public:
    /// Constructs a reader from a filename.
    ///
    /// :param filename: Path to a Maestro file (.mae, .mae.gz, .maegz).
    /// :param config: Reader configuration for tag format and perception.
    /// :raises MaestroParseError: If the file cannot be opened.
    explicit OEMaestroDesignUnitReader(const std::string& filename,
                                       OEMaestroReaderConfig config = {});

    /// Constructs a reader from an OpenEye oeifstream.
    ///
    /// :param ifs: An open OpenEye input file stream.
    /// :param config: Reader configuration for tag format and perception.
    /// :raises MaestroParseError: If the stream is invalid.
    explicit OEMaestroDesignUnitReader(OEPlatform::oeifstream& ifs,
                                       OEMaestroReaderConfig config = {});

    /// Reads the next CT block as a design unit.
    ///
    /// :param du: OEDesignUnit to populate.
    /// :returns: True if a design unit was read, false at EOF.
    bool Read(OEBio::OEDesignUnit& du);

    /// Overrides the default ligand atom predicate.
    ///
    /// :param pred: Unary atom predicate that returns true for ligand atoms.
    void SetLigandPredicate(const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred);

    /// Overrides the default solvent atom predicate.
    ///
    /// :param pred: Unary atom predicate that returns true for solvent atoms.
    void SetSolventPredicate(const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred);

    /// Overrides the default cofactor atom predicate.
    ///
    /// :param pred: Unary atom predicate that returns true for cofactor atoms.
    void SetCofactorPredicate(const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred);

    void SetPerception(OEMaestroPerception perception);
    void SetTagFormat(OEMaestroTag tags);
    OEMaestroPerception GetPerception() const;
    OEMaestroTag GetTagFormat() const;
    OEMaestroReaderConfig GetConfig() const;

    ~OEMaestroDesignUnitReader();
    OEMaestroDesignUnitReader(const OEMaestroDesignUnitReader&) = delete;
    OEMaestroDesignUnitReader& operator=(const OEMaestroDesignUnitReader&) = delete;
    OEMaestroDesignUnitReader(OEMaestroDesignUnitReader&&) noexcept;
    OEMaestroDesignUnitReader& operator=(OEMaestroDesignUnitReader&&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEMAESTRODESIGNUNITREADER_H
