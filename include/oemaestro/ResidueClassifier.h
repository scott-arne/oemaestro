#ifndef OEMAESTRO_RESIDUECLASSIFIER_H
#define OEMAESTRO_RESIDUECLASSIFIER_H

#include <string>
#include <unordered_set>

namespace OEMaestro {

/// Returns the set of PDB solvent residue codes.
///
/// Excludes metal cations and simple anions (these stay in the protein component).
/// Includes: water, organic solvents, cryoprotectants, buffers, detergents.
///
/// :returns: Const reference to the solvent code set.
const std::unordered_set<std::string>& GetSolventCodes();

/// Returns the set of PDB cofactor residue codes.
///
/// Includes: nucleotide cofactors, coenzymes, hemes, iron-sulfur clusters,
/// sugar cofactors, lipid cofactors, and other common cofactors.
///
/// :returns: Const reference to the cofactor code set.
const std::unordered_set<std::string>& GetCofactorCodes();

/// Returns true if the residue name (after whitespace stripping) is a known solvent.
///
/// :param resname: PDB residue name (leading/trailing whitespace is stripped).
/// :returns: True if the residue is a known solvent.
bool IsSolventResidue(const std::string& resname);

/// Returns true if the residue name (after whitespace stripping) is a known cofactor.
///
/// :param resname: PDB residue name (leading/trailing whitespace is stripped).
/// :returns: True if the residue is a known cofactor.
bool IsCofactorResidue(const std::string& resname);

}  // namespace OEMaestro

#endif  // OEMAESTRO_RESIDUECLASSIFIER_H
