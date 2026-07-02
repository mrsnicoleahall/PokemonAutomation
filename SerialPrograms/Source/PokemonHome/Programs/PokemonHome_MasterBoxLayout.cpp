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
        {"Utility",        BoxCategory::Utility},   // v2
        // v3 additions
        {"ShinyDex",       BoxCategory::ShinyDex},
        {"RegularDex",     BoxCategory::RegularDex},
        {"ShinyTrades",    BoxCategory::ShinyTrades},
        {"RegularTrades",  BoxCategory::RegularTrades},
        {"Junk",           BoxCategory::Junk},
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


// ---------------------------------------------------------------------------
// v3 loader — loads master_box_layout_v3.json + shiny_locked.json.
// Mirrors the v1 loader idiom exactly.
// ---------------------------------------------------------------------------
MasterBoxLayoutV3 load_master_box_layout_v3(
    const std::string& layout_path,
    const std::string& shiny_locked_path
){
    // --- layout file ---
    JsonValue layout_json = load_json_file(layout_path);
    const JsonObject& root = layout_json.to_object_throw(layout_path);

    MasterBoxLayoutV3 layout;

    layout.shiny_dex_start =
        static_cast<uint16_t>(root.get_integer_throw("shiny_dex_start", layout_path));
    layout.regular_dex_start =
        static_cast<uint16_t>(root.get_integer_throw("regular_dex_start", layout_path));
    layout.shiny_dex_buffer_boxes =
        static_cast<uint16_t>(root.get_integer_throw("shiny_dex_buffer_boxes", layout_path));
    layout.regular_dex_buffer_boxes =
        static_cast<uint16_t>(root.get_integer_throw("regular_dex_buffer_boxes", layout_path));

    // category_box_ranges: object mapping category-name -> [start, end]
    {
        const JsonObject& ranges_obj = root.get_object_throw("category_box_ranges", layout_path);
        for (auto it = ranges_obj.begin(); it != ranges_obj.end(); ++it){
            const std::string& cat_name = it->first;
            const JsonArray& pair_arr   = it->second.to_array_throw(layout_path);
            if (pair_arr.size() != 2){
                throw std::runtime_error(
                    "master_box_layout_v3.json: category \"" + cat_name
                    + "\" range must be a 2-element array in " + layout_path
                );
            }
            int64_t start_val = 0, end_val = 0;
            if (!pair_arr[0].read_integer(start_val) || !pair_arr[1].read_integer(end_val)){
                throw std::runtime_error(
                    "master_box_layout_v3.json: non-integer range bound in category \""
                    + cat_name + "\" in " + layout_path
                );
            }
            BoxCategory cat = parse_category(cat_name, layout_path);
            layout.category_box_ranges[cat] = {
                static_cast<uint16_t>(start_val),
                static_cast<uint16_t>(end_val)
            };
        }
    }

    // legendary / mythical / ultra_beast / paradox dex-number sets
    layout.legendary   = parse_dex_set(root.get_array_throw("legendary",   layout_path), layout_path);
    layout.mythical     = parse_dex_set(root.get_array_throw("mythical",    layout_path), layout_path);
    layout.ultra_beast  = parse_dex_set(root.get_array_throw("ultra_beast", layout_path), layout_path);
    layout.paradox      = parse_dex_set(root.get_array_throw("paradox",     layout_path), layout_path);

    // --- shiny_locked file ---
    JsonValue sl_json = load_json_file(shiny_locked_path);
    const JsonObject& sl_root = sl_json.to_object_throw(shiny_locked_path);
    layout.shiny_locked = parse_dex_set(
        sl_root.get_array_throw("shiny_locked", shiny_locked_path),
        shiny_locked_path
    );

    return layout;
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
