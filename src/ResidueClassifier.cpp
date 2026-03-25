#include "oemaestro/ResidueClassifier.h"

namespace OEMaestro {

static std::string StripWhitespace(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

const std::unordered_set<std::string>& GetSolventCodes() {
    // From data/pdb_solvent_codes.txt
    // Includes: WATER, ORGANIC SOLVENTS, CRYOPROTECTANTS & POLYOLS,
    //           BUFFER MOLECULES, DETERGENTS & ADDITIVES
    // Excludes: COMMON ANIONS, COMMON CATIONS (metals/ions stay in protein)
    static const std::unordered_set<std::string> codes = {
        // WATER
        "HOH", "DOD",
        // ORGANIC SOLVENTS
        "MOH", "EOH", "IPA", "1BO", "TBU", "SBT", "ACE", "ACN", "CCN",
        "DMS", "DMF", "THF", "DOX", "DIO", "BEN", "TOL", "CXE", "12P",
        "TCE", "MRD",
        // CRYOPROTECTANTS & POLYOLS
        "GOL", "EDO", "MPD", "PGE", "PEG", "PG4", "PGO", "P6G", "1PE",
        "2PE", "P33", "PG0", "15P", "XPE", "PDO", "PIG", "BU1", "BU2",
        "BU3",
        // BUFFER MOLECULES
        "TRS", "MES", "EPE", "CIT", "TAR", "IMD", "CAC", "POP", "MPO",
        "ADA", "BIC", "NHE", "BTB", "B3P",
        // DETERGENTS & ADDITIVES
        "BME", "DTT", "SDS", "LDA", "BOG", "OLC", "PLM", "MYR", "LMT",
        "C8E", "UNX", "UNL",
    };
    return codes;
}

const std::unordered_set<std::string>& GetCofactorCodes() {
    // From data/pdb_cofactor_codes.txt
    static const std::unordered_set<std::string> codes = {
        // NUCLEOTIDE COFACTORS
        "NAD", "NAI", "NAP", "NDP", "FAD", "FMN", "ATP", "ADP", "AMP",
        "GTP", "GDP", "GMP", "CTP", "CDP", "CMP", "UTP", "UDP", "UMP",
        "TTP", "ANP", "ACP", "AGS", "GNP", "GSP", "ADN",
        // COENZYMES & VITAMIN-DERIVED COFACTORS
        "COA", "ACO", "SAM", "SAH", "PLP", "PMP", "TPP", "THM", "BTN",
        "H4B", "THG", "FOL", "MTX", "ASC", "RBF", "NCN", "NCA", "UQ",
        "MQ7", "PHQ", "RET", "REA",
        // HEME & PORPHYRINS
        "HEM", "HEC", "HEA", "HEB", "BCL", "CLA", "CLB", "BPH", "PHO",
        "SRM",
        // IRON-SULFUR CLUSTER LIGANDS
        "SF4", "FES", "F3S",
        // SUGAR NUCLEOTIDES & SUGAR COFACTORS
        // (UDP already listed above)
        "A2G", "NAG", "BMA", "MAN", "GAL", "GLC", "FUC", "SIA", "BGC",
        // LIPID COFACTORS
        "LPX", "CDL", "PLC", "PE", "PS",
        // OTHER COMMON COFACTORS & SUBSTRATES
        "GSH", "GDS", "HEZ", "CRO", "AKG", "OAA", "PYR", "MAL", "FUM",
        "CIT", "IPE", "FPP", "GPP",
    };
    return codes;
}

bool IsSolventResidue(const std::string& resname) {
    return GetSolventCodes().count(StripWhitespace(resname)) > 0;
}

bool IsCofactorResidue(const std::string& resname) {
    return GetCofactorCodes().count(StripWhitespace(resname)) > 0;
}

}  // namespace OEMaestro
