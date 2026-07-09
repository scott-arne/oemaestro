#ifndef OEMAESTRO_MOLCONVERTER_H
#define OEMAESTRO_MOLCONVERTER_H

#include <vector>
#include <oechem.h>
#include "oemaestro/Enums.h"
#include "oemaestro/MaestroMol.h"
#include "oemaestro/OEMaestroTagConverter.h"

namespace OEMaestro {

/// Converts between MaestroMol IR and OpenEye OEMolBase.
///
/// The two directions have distinct method names rather than overloads of a
/// single Convert. The write overload (MaestroMol, OEMolBase) would otherwise be
/// indistinguishable from a read call written with the arguments transposed, and
/// SWIG's pointer-based overload resolution would silently bind such a call to
/// the wrong direction. Argument order follows OpenEye convention: (dst, src).
class MolConverter {
public:
    MolConverter();
    explicit MolConverter(OEMaestroTag tags,
                          OEMaestroPerception perception = PERCEPTION_ALL);

    /// Read direction: MaestroMol -> OEMolBase.
    void ConvertToOE(OEChem::OEMolBase& dst, const MaestroMol& src) const;

    /// Write direction: OEMolBase -> MaestroMol (active conformer).
    void ConvertToMaestro(MaestroMol& dst, const OEChem::OEMolBase& src) const;

    /// Write direction: OEMolBase -> vector of MaestroMol (one per conformer).
    ///
    /// If src is a multi-conformer molecule (OEMCMolBase), produces one MaestroMol
    /// per conformer. Otherwise produces a single MaestroMol from the active conformer.
    void ConvertToMaestro(std::vector<MaestroMol>& dst, const OEChem::OEMolBase& src) const;

    void SetTagFormat(OEMaestroTag tags);
    OEMaestroTag GetTagFormat() const;          // NOLINT(modernize-use-nodiscard)
    void SetPerception(OEMaestroPerception perception);
    OEMaestroPerception GetPerception() const;  // NOLINT(modernize-use-nodiscard)

private:
    void ApplyDataTags(const MaestroMol& maestro_mol,
                       OEChem::OEMolBase& mol) const;
    void RunPerception(OEChem::OEMolBase& mol, int dimension) const;

    OEMaestroTagConverter tag_converter_;
    OEMaestroPerception perception_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MOLCONVERTER_H
