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
/// Handles both read direction (MaestroMol -> OEMolBase) and write direction
/// (OEMolBase -> MaestroMol). Argument order follows OpenEye convention: (dst, src).
class MolConverter {
public:
    MolConverter();
    explicit MolConverter(OEMaestroTag tags,
                          OEMaestroPerception perception = PERCEPTION_ALL);

    /// Read direction: MaestroMol -> OEMolBase.
    void Convert(OEChem::OEMolBase& dst, const MaestroMol& src) const;

    void SetTagFormat(OEMaestroTag tags);
    OEMaestroTag GetTagFormat() const;
    void SetPerception(OEMaestroPerception perception);
    OEMaestroPerception GetPerception() const;

private:
    void ApplyDataTags(const MaestroMol& maestro_mol,
                       OEChem::OEMolBase& mol) const;
    void RunPerception(OEChem::OEMolBase& mol, int dimension) const;

    OEMaestroTagConverter tag_converter_;
    OEMaestroPerception perception_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MOLCONVERTER_H
