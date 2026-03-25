#include "oemaestro/OEMaestroDesignUnitReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/ResidueClassifier.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"

namespace OEMaestro {

// --- Default predicates ---

class IsLigandAtom : public OESystem::OEUnaryPredicate<OEChem::OEAtomBase> {
public:
    bool operator()(const OEChem::OEAtomBase& atom) const override {
        return atom.GetBoolData("is_ligand_atom");
    }
    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* CreateCopy() const override {
        return new IsLigandAtom(*this);
    }
};

class IsSolventAtom : public OESystem::OEUnaryPredicate<OEChem::OEAtomBase> {
public:
    bool operator()(const OEChem::OEAtomBase& atom) const override {
        OEChem::OEResidue res = OEChem::OEAtomGetResidue(&atom);
        return IsSolventResidue(res.GetName());
    }
    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* CreateCopy() const override {
        return new IsSolventAtom(*this);
    }
};

class IsCofactorAtom : public OESystem::OEUnaryPredicate<OEChem::OEAtomBase> {
public:
    bool operator()(const OEChem::OEAtomBase& atom) const override {
        OEChem::OEResidue res = OEChem::OEAtomGetResidue(&atom);
        std::string name = res.GetName();
        // Cofactor only if NOT already classified as solvent (solvent has priority)
        return IsCofactorResidue(name) && !IsSolventResidue(name);
    }
    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* CreateCopy() const override {
        return new IsCofactorAtom(*this);
    }
};

// --- Impl ---

struct OEMaestroDesignUnitReader::Impl {
    std::unique_ptr<MaestroReader> reader;
    MolConverter converter;
    MaestroMol maestro_buf;
    std::unique_ptr<StreamAdapter> stream_adapter;

    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* ligand_pred;
    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* solvent_pred;
    OESystem::OEUnaryPredicate<OEChem::OEAtomBase>* cofactor_pred;

    Impl(const std::string& filename, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          ligand_pred(new IsLigandAtom()),
          solvent_pred(new IsSolventAtom()),
          cofactor_pred(new IsCofactorAtom()) {
        reader = std::make_unique<MaestroReader>(filename);
    }

    Impl(OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          ligand_pred(new IsLigandAtom()),
          solvent_pred(new IsSolventAtom()),
          cofactor_pred(new IsCofactorAtom()) {
        stream_adapter = std::make_unique<StreamAdapter>(ifs);
        auto stream_ptr = std::shared_ptr<std::istream>(
            stream_adapter.get(), [](std::istream*) {});
        reader = std::make_unique<MaestroReader>(stream_ptr);
    }

    ~Impl() {
        delete ligand_pred;
        delete solvent_pred;
        delete cofactor_pred;
    }

    bool Read(OEBio::OEDesignUnit& du) {
        if (!reader->Read(maestro_buf))
            return false;

        OEChem::OEGraphMol full_mol;
        converter.Convert(maestro_buf, full_mol);

        // Build the protein predicate: NOT(ligand OR solvent OR cofactor)
        OESystem::OEOr<OEChem::OEAtomBase> non_protein(*ligand_pred, *solvent_pred);
        OESystem::OEOr<OEChem::OEAtomBase> any_component(non_protein, *cofactor_pred);
        OESystem::OENot<OEChem::OEAtomBase> protein_pred(any_component);

        // Split into components
        OEChem::OEGraphMol protein, ligand, solvent, cofactors;
        OEChem::OESubsetMol(protein, full_mol, protein_pred);
        OEChem::OESubsetMol(ligand, full_mol, *ligand_pred);
        OEChem::OESubsetMol(solvent, full_mol, *solvent_pred);
        OEChem::OESubsetMol(cofactors, full_mol, *cofactor_pred);

        // Preserve title on the protein component
        protein.SetTitle(full_mol.GetTitle());

        du = OEBio::OEDesignUnit(protein, ligand, solvent, cofactors,
                                  OEBio::OEDesignUnitComponents::Protein);
        return true;
    }
};

OEMaestroDesignUnitReader::OEMaestroDesignUnitReader(
    const std::string& filename, OEMaestroReaderConfig config)
    : pimpl_(std::make_unique<Impl>(filename, config)) {}

OEMaestroDesignUnitReader::OEMaestroDesignUnitReader(
    OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
    : pimpl_(std::make_unique<Impl>(ifs, config)) {}

bool OEMaestroDesignUnitReader::Read(OEBio::OEDesignUnit& du) {
    return pimpl_->Read(du);
}

void OEMaestroDesignUnitReader::SetLigandPredicate(
    const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred) {
    delete pimpl_->ligand_pred;
    pimpl_->ligand_pred = static_cast<OESystem::OEUnaryPredicate<OEChem::OEAtomBase>*>(pred.CreateCopy());
}

void OEMaestroDesignUnitReader::SetSolventPredicate(
    const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred) {
    delete pimpl_->solvent_pred;
    pimpl_->solvent_pred = static_cast<OESystem::OEUnaryPredicate<OEChem::OEAtomBase>*>(pred.CreateCopy());
}

void OEMaestroDesignUnitReader::SetCofactorPredicate(
    const OESystem::OEUnaryPredicate<OEChem::OEAtomBase>& pred) {
    delete pimpl_->cofactor_pred;
    pimpl_->cofactor_pred = static_cast<OESystem::OEUnaryPredicate<OEChem::OEAtomBase>*>(pred.CreateCopy());
}

void OEMaestroDesignUnitReader::SetPerception(OEMaestroPerception perception) {
    pimpl_->converter.SetPerception(perception);
}

void OEMaestroDesignUnitReader::SetTagFormat(OEMaestroTag tags) {
    pimpl_->converter.SetTagFormat(tags);
}

OEMaestroPerception OEMaestroDesignUnitReader::GetPerception() const {
    return pimpl_->converter.GetPerception();
}

OEMaestroTag OEMaestroDesignUnitReader::GetTagFormat() const {
    return pimpl_->converter.GetTagFormat();
}

OEMaestroReaderConfig OEMaestroDesignUnitReader::GetConfig() const {
    return {pimpl_->converter.GetTagFormat(), pimpl_->converter.GetPerception()};
}

OEMaestroDesignUnitReader::~OEMaestroDesignUnitReader() = default;
OEMaestroDesignUnitReader::OEMaestroDesignUnitReader(
    OEMaestroDesignUnitReader&&) noexcept = default;
OEMaestroDesignUnitReader& OEMaestroDesignUnitReader::operator=(
    OEMaestroDesignUnitReader&&) noexcept = default;

}  // namespace OEMaestro
