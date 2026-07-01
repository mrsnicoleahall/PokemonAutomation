/*  Catalogue CSV Export
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Free functions to format the scanned catalogue as a CSV file.
 *  No Qt, no hardware, no file IO — intentionally pure so it can be unit-tested
 *  in isolation.
 */

#ifndef PokemonAutomation_PokemonHome_CatalogueCsv_H
#define PokemonAutomation_PokemonHome_CatalogueCsv_H

#include <string>
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// Return the header line (28 columns) including a trailing '\n'.
std::string catalogue_csv_header();

// Return a data row for one catalogue entry, including a trailing '\n'.
// box/row/col are 0-indexed and will be output as 1-indexed (add 1).
// dest_box is the 0-indexed absolute HOME box number (-1 if unplaceable) — output as-is.
std::string catalogue_csv_row(
    size_t box,
    size_t row,
    size_t col,
    const Pokemon::CollectedPokemonInfo& info,
    const std::string& routed_category,
    int dest_box
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif // PokemonAutomation_PokemonHome_CatalogueCsv_H
