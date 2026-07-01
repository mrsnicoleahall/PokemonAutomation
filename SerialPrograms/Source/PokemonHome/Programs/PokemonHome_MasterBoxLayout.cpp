/*  Master Box Layout
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <map>
#include <set>
#include <string>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "PokemonHome_MasterBoxLayout.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Internal: parse a JSON array of integers into a set<uint16_t>.
// ---------------------------------------------------------------------------
static std::set<uint16_t> parse_dex_set(const JsonArray& arr, const std::string& path){
    std::set<uint16_t> result;
    for (const JsonValue& v : arr){
        int64_t n = 0;
        if (!v.read_integer(n)){
            throw std::runtime_error("master_box_layout.json: dex-number array contains non-integer in " + path);
        }
        result.insert(static_cast<uint16_t>(n));
    }
    return result;
}


// ---------------------------------------------------------------------------
// Internal: map a category name string to BoxCategory enum.
// Throws if the string is not recognised.
// ---------------------------------------------------------------------------
static BoxCategory parse_category(const std::string& name, const std::string& path){
    static const std::map<std::string, BoxCategory> TABLE{
        {"LivingDex",      BoxCategory::LivingDex},
        {"Competitive",    BoxCategory::Competitive},
        {"Breeding",       BoxCategory::Breeding},
        {"Breedject",      BoxCategory::Breedject},
        {"Events",         BoxCategory::Events},
        {"GoodTrades",     BoxCategory::GoodTrades},
        {"DuplicateShiny", BoxCategory::DuplicateShiny},
        {"Legendary",      BoxCategory::Legendary},
        {"Mythical",       BoxCategory::Mythical},
        {"UltraBeast",     BoxCategory::UltraBeast},
        {"Paradox",        BoxCategory::Paradox},
        {"ManualForms",    BoxCategory::ManualForms},
        {"ManualOther",    BoxCategory::ManualOther},
    };
    auto it = TABLE.find(name);
    if (it == TABLE.end()){
        throw std::runtime_error(
            "master_box_layout.json: unknown category \"" + name + "\" in " + path
        );
    }
    return it->second;
}


// ---------------------------------------------------------------------------
// Public loader.
// ---------------------------------------------------------------------------
MasterBoxLayout load_master_box_layout(const std::string& path){
    JsonValue json = load_json_file(path);
    const JsonObject& root = json.to_object_throw(path);

    MasterBoxLayout layout;

    // living_dex_start_box
    layout.living_dex_start_box =
        static_cast<uint16_t>(root.get_integer_throw("living_dex_start_box", path));

    // category_box_ranges: object mapping category-name -> [start, end]
    {
        const JsonObject& ranges_obj = root.get_object_throw("category_box_ranges", path);
        for (auto it = ranges_obj.begin(); it != ranges_obj.end(); ++it){
            const std::string& cat_name = it->first;
            const JsonArray& pair_arr   = it->second.to_array_throw(path);
            if (pair_arr.size() != 2){
                throw std::runtime_error(
                    "master_box_layout.json: category \"" + cat_name
                    + "\" range must be a 2-element array in " + path
                );
            }
            int64_t start_val = 0, end_val = 0;
            if (!pair_arr[0].read_integer(start_val) || !pair_arr[1].read_integer(end_val)){
                throw std::runtime_error(
                    "master_box_layout.json: non-integer range bound in category \""
                    + cat_name + "\" in " + path
                );
            }
            BoxCategory cat = parse_category(cat_name, path);
            layout.category_box_ranges[cat] = {
                static_cast<uint16_t>(start_val),
                static_cast<uint16_t>(end_val)
            };
        }
    }

    // legendary / mythical / ultra_beast / paradox dex-number sets
    layout.legendary   = parse_dex_set(root.get_array_throw("legendary",   path), path);
    layout.mythical     = parse_dex_set(root.get_array_throw("mythical",    path), path);
    layout.ultra_beast  = parse_dex_set(root.get_array_throw("ultra_beast", path), path);
    layout.paradox      = parse_dex_set(root.get_array_throw("paradox",     path), path);

    return layout;
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
