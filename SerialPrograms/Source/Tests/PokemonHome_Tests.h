/*  PokemonHome Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *
 */


#ifndef PokemonAutomation_Tests_PokemonHome_Tests_H
#define PokemonAutomation_Tests_PokemonHome_Tests_H

#include <vector>
#include <string>

namespace PokemonAutomation{

class ImageViewRGB32;


int test_pokemonHome_BoxView(const ImageViewRGB32& image, const std::vector<std::string>& keywords);

int test_pokemonHome_SummaryScreen(const ImageViewRGB32& image, const std::vector<std::string>& keywords);

int test_pokemonHome_IvSummary(const ImageViewRGB32& image);

int test_pokemonHome_MasterBoxRouter(const ImageViewRGB32& image);

int test_pokemonHome_MasterBoxLayout(const ImageViewRGB32& image);

int test_pokemonHome_MasterPlanner(const ImageViewRGB32& image);

int test_pokemonHome_UtilityPlan(const ImageViewRGB32& image);

int test_pokemonHome_UtilityRouting(const ImageViewRGB32& image);

int test_pokemonHome_CatalogueCsv(const ImageViewRGB32& image);

}

#endif
