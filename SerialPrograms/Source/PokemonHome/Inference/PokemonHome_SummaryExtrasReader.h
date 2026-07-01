/*  Summary Extras Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads ability, nature, and held item from the HOME summary screen.
 *  The summary screen is the same screen the box sorter already visits;
 *  no additional navigation is required.
 *
 *  NOTE: All crop box coordinates (ABILITY_BOX, NATURE_BOX, HELD_ITEM_BOX)
 *  are calibrated on the physical capture rig (Task 7). HELD_ITEM_BOX in
 *  particular is a placeholder — measure it before enabling held-item routing.
 *
 */

#ifndef PokemonAutomation_PokemonHome_SummaryExtrasReader_H
#define PokemonAutomation_PokemonHome_SummaryExtrasReader_H

#include <string>
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"

namespace PokemonAutomation{
    class VideoStream;
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Crop boxes — all coordinates calibrated on the capture rig (Task 7).
// HELD_ITEM_BOX is a placeholder; verify on hardware before relying on it.
// ---------------------------------------------------------------------------
extern const ImageFloatBox ABILITY_BOX;     // {0.158, 0.838, 0.213, 0.042}
extern const ImageFloatBox NATURE_BOX;      // {0.157, 0.783, 0.212, 0.042}
extern const ImageFloatBox HELD_ITEM_BOX;   // {0.157, 0.728, 0.212, 0.042} — PLACEHOLDER


// ---------------------------------------------------------------------------
// Result struct
// ---------------------------------------------------------------------------
struct SummaryExtras{
    // Normalized slug (lowercase, alphanumeric only) produced by
    // OCR::normalize_utf32 + utf32_to_str.  The router (Task 2) does the
    // targeted match against the Utility target lists.
    std::string ability_slug;

    // Token returned by the NatureCheckerOCR dictionary matcher,
    // e.g. "adamant", "timid".  Empty string if OCR fails.
    std::string nature;

    // Normalized slug for the held item, same scheme as ability_slug.
    // Empty string if no item or OCR fails.
    std::string held_item_slug;
};


// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

// Read ability, nature, and held item from the currently displayed HOME
// summary screen.  `stream` is used for logging only; `screen` must be a
// snapshot already taken of the summary screen — no navigation is performed.
SummaryExtras read_summary_extras(
    VideoStream& stream,
    const VideoSnapshot& screen,
    Language language
);

// Register the three crop boxes with the overlay so they are visible in the
// video feed UI while the program runs.
void make_summary_extras_overlays(VideoOverlaySet& set);



}
}
}
#endif
