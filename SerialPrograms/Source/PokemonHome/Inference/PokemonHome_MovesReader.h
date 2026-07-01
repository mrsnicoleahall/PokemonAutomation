/*  Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads a Pokémon's four moves from the HOME moves screen.
 *
 *  NAVIGATION NOTE: This class only reads a snapshot of the moves screen.
 *  Opening and closing the moves screen (pressing the button that toggles it)
 *  is the responsibility of the calling program (Task 6). Pass in a
 *  snapshot already taken of the moves screen — no controller input is
 *  performed here.
 *
 *  CALIBRATION NOTE: The four move crop boxes are placeholder coordinates
 *  seeded from a reasonable estimate of the HOME moves-screen layout.
 *  Exact coordinates must be calibrated on the physical capture rig (Task 7).
 *
 */

#ifndef PokemonAutomation_PokemonHome_MovesReader_H
#define PokemonAutomation_PokemonHome_MovesReader_H

#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Static dictionary matcher for move names.
// Bound to "PokemonHome/PokemonMovesOCR.json".
// ---------------------------------------------------------------------------
const OCR::SmallDictionaryMatcher& MOVES_READER();


// ---------------------------------------------------------------------------
// MovesReaderScope
//
// Attach four overlay crop boxes to the video feed and expose read() /
// dump_images() for use while the moves screen is open.
//
// Usage:
//   1. The program (Task 6) navigates to the moves screen.
//   2. Caller constructs MovesReaderScope (boxes appear in the overlay).
//   3. Caller calls read(logger, frame) with a snapshot of the moves screen.
//   4. The program navigates away; the scope destructor removes the overlays.
//
// The four move slots are laid out vertically on the right side of the screen.
// Crop boxes are PLACEHOLDERS — calibrate on the capture rig in Task 7.
// ---------------------------------------------------------------------------
class MovesReaderScope{
public:
    MovesReaderScope(VideoOverlay& overlay, Language language);

    // OCR-match each of the 4 move slots against the dictionary.
    // Returns normalized move slugs for successfully matched slots (up to 4).
    // Slots that are empty or fail to match are omitted from the result.
    std::vector<std::string> read(Logger& logger, const ImageViewRGB32& frame);

    // Return the four raw crop images (in slot order: move 1..4).
    // Useful for debugging crop placement before hardware calibration.
    std::vector<ImageViewRGB32> dump_images(const ImageViewRGB32& frame);

private:
    // Read a single slot; returns empty string on OCR failure.
    std::string read_one(Logger& logger, const ImageViewRGB32& frame, const OverlayBoxScope& box, int slot_index);

private:
    Language m_language;
    // PLACEHOLDER crop boxes — vertical stack on the right side of the moves screen.
    // Coordinates: {x, y, w, h} as fractions of the full frame.
    // Calibrate all four on hardware (Task 7) before shipping.
    OverlayBoxScope m_box_move1;  // {0.62, 0.22, 0.28, 0.06}
    OverlayBoxScope m_box_move2;  // {0.62, 0.34, 0.28, 0.06}
    OverlayBoxScope m_box_move3;  // {0.62, 0.46, 0.28, 0.06}
    OverlayBoxScope m_box_move4;  // {0.62, 0.58, 0.28, 0.06}
};


}
}
}
#endif
