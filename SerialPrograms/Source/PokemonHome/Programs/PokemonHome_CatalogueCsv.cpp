/*  Catalogue CSV Export
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <sstream>
#include <string>
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "Pokemon/Pokemon_OriginMarks.h"
#include "Pokemon/Pokemon_Types.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "PokemonHome_CatalogueCsv.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// Internal helper: CSV-escape a single field value.
// If the value contains a comma, double-quote, or newline, wrap it in double
// quotes and double any internal double-quote characters.
// ---------------------------------------------------------------------------
static std::string csv_escape(const std::string& s){
    // Check if escaping is needed.
    bool needs_quotes = false;
    for (char c : s){
        if (c == ',' || c == '"' || c == '\n'){
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes){
        return s;
    }
    // Wrap in double quotes, doubling any internal double-quote characters.
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s){
        if (c == '"'){
            out += '"'; // double the quote
        }
        out += c;
    }
    out += '"';
    return out;
}


// ---------------------------------------------------------------------------
// catalogue_csv_header
// ---------------------------------------------------------------------------
std::string catalogue_csv_header(){
    return "box,row,col,dex,species,shiny,gender,alpha,gmax,ball,"
           "ot_name,ot_id,type1,type2,tera_type,origin_mark,"
           "iv_read,iv_best_count,iv_total_estimate,iv_perfect,"
           "ability,nature,held_item,moves,extras_read,moves_read,"
           "routed_category,dest_box\n";
}


// ---------------------------------------------------------------------------
// catalogue_csv_row
// ---------------------------------------------------------------------------
std::string catalogue_csv_row(
    size_t box,
    size_t row,
    size_t col,
    const Pokemon::CollectedPokemonInfo& info,
    const std::string& routed_category,
    int dest_box
){
    // Helper: bool to string.
    auto bool_str = [](bool b) -> const char* { return b ? "true" : "false"; };

    // Helper: type slug — return empty string for NONE.
    auto type_slug = [](const std::string& s) -> std::string {
        if (s.empty() || s == "none"){
            return "";
        }
        return s;
    };

    // Moves joined by pipe.
    std::string moves_str;
    for (size_t i = 0; i < info.moves.size(); i++){
        if (i > 0){ moves_str += '|'; }
        moves_str += info.moves[i];
    }

    // Build 28 fields.
    std::ostringstream oss;
    oss << (box + 1)                                                             // 0  box
        << ',' << (row + 1)                                                      // 1  row
        << ',' << (col + 1)                                                      // 2  col
        << ',' << info.dex_number                                                // 3  dex
        << ',' << csv_escape(info.name_slug)                                     // 4  species
        << ',' << bool_str(info.shiny)                                           // 5  shiny
        << ',' << csv_escape(gender_to_string(info.gender))                      // 6  gender
        << ',' << bool_str(info.alpha)                                           // 7  alpha
        << ',' << bool_str(info.gmax)                                            // 8  gmax
        << ',' << csv_escape(info.ball_slug)                                     // 9  ball
        << ',' << csv_escape(info.ot_name)                                       // 10 ot_name
        << ',' << info.ot_id                                                     // 11 ot_id
        << ',' << type_slug(POKEMON_TYPE_SLUGS().get_string(info.primary_type))  // 12 type1
        << ',' << type_slug(POKEMON_TYPE_SLUGS().get_string(info.secondary_type))// 13 type2
        << ',' << type_slug(POKEMON_TERA_TYPE_SLUGS().get_string(info.tera_type))// 14 tera_type
        << ',' << csv_escape(ORIGIN_MARK_SLUGS().get_string(info.origin_mark))  // 15 origin_mark
        << ',' << bool_str(info.iv_read)                                         // 16 iv_read
        << ',' << static_cast<int>(info.iv_best_count)                           // 17 iv_best_count
        << ',' << info.iv_total_estimate                                         // 18 iv_total_estimate
        << ',' << bool_str(info.iv_perfect)                                      // 19 iv_perfect
        << ',' << csv_escape(info.ability_slug)                                  // 20 ability
        << ',' << csv_escape(info.nature)                                        // 21 nature
        << ',' << csv_escape(info.held_item_slug)                                // 22 held_item
        << ',' << csv_escape(moves_str)                                          // 23 moves
        << ',' << bool_str(info.extras_read)                                     // 24 extras_read
        << ',' << bool_str(info.moves_read)                                      // 25 moves_read
        << ',' << csv_escape(routed_category)                                    // 26 routed_category
        << ',' << dest_box                                                        // 27 dest_box
        << '\n';

    return oss.str();
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
