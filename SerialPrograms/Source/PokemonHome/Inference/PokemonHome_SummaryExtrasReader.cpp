/*  Summary Extras Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Strings/Unicode.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "CommonTools/OCR/OCR_StringNormalization.h"
#include "Pokemon/Inference/Pokemon_IvJudgeReader.h"
#include "PokemonHome_SummaryExtrasReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Crop boxes
// All coordinates confirmed on the physical capture rig (Task 7).
// HELD_ITEM_BOX is a placeholder — measure on hardware before enabling
// held-item routing in Task 7.
// ---------------------------------------------------------------------------
const ImageFloatBox ABILITY_BOX   {0.158, 0.838, 0.213, 0.042};
const ImageFloatBox NATURE_BOX    {0.157, 0.783, 0.212, 0.042};
const ImageFloatBox HELD_ITEM_BOX {0.157, 0.728, 0.212, 0.042}; // PLACEHOLDER: calibrate on rig


// ---------------------------------------------------------------------------
// Nature dictionary matcher — mirrors IvJudgeReader's static-reader idiom.
// ---------------------------------------------------------------------------
namespace{

const OCR::SmallDictionaryMatcher& NATURE_READER(){
    static const OCR::SmallDictionaryMatcher reader("Pokemon/NatureCheckerOCR.json");
    return reader;
}

} // anonymous namespace


// ---------------------------------------------------------------------------
// Overlay helper
// ---------------------------------------------------------------------------
void make_summary_extras_overlays(VideoOverlaySet& set){
    set.add(COLOR_RED,   ABILITY_BOX,   "Ability");
    set.add(COLOR_GREEN, NATURE_BOX,    "Nature");
    set.add(COLOR_BLUE,  HELD_ITEM_BOX, "HeldItem");
}


// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace{

// OCR a single box and return the normalized slug.
// Returns an empty string on OCR failure.
std::string ocr_slug(
    VideoStream& stream,
    const ImageViewRGB32& frame,
    const ImageFloatBox& box,
    Language language,
    const char* field_name
){
    ImageViewRGB32 image = extract_box_reference(frame, box);
    std::string raw = OCR::ocr_read(language, image, OCR::PageSegMode::SINGLE_LINE);
    stream.log(std::string(field_name) + " raw OCR: " + raw);
    std::string slug = utf32_to_str(OCR::normalize_utf32(raw));
    stream.log(std::string(field_name) + " slug: " + slug);
    return slug;
}

// Read nature via the SmallDictionaryMatcher; returns the matched token
// (e.g. "adamant") or an empty string if the match fails.
std::string read_nature(
    VideoStream& stream,
    const ImageViewRGB32& frame,
    Language language
){
    ImageViewRGB32 image = extract_box_reference(frame, NATURE_BOX);
    OCR::StringMatchResult result = NATURE_READER().match_substring_from_image_multifiltered(
        &stream.logger(),
        language,
        image,
        OCR::WHITE_TEXT_FILTERS(),
        Pokemon::IvJudgeReader::MAX_LOG10P,
        Pokemon::IvJudgeReader::MAX_LOG10P_SPREAD
    );
    result.clear_beyond_log10p(Pokemon::IvJudgeReader::MAX_LOG10P);
    if (result.results.size() != 1){
        stream.log("SummaryExtrasReader: nature OCR failed or ambiguous");
        return "";
    }
    const std::string& token = result.results.begin()->second.token;
    stream.log("Nature: " + token);
    return token;
}

} // anonymous namespace


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
SummaryExtras read_summary_extras(
    VideoStream& stream,
    const VideoSnapshot& screen,
    Language language
){
    SummaryExtras extras;

    if (!screen){
        stream.log("SummaryExtrasReader: snapshot is null, skipping");
        return extras;
    }

    const ImageViewRGB32 frame = static_cast<ImageViewRGB32>(screen);

    extras.nature        = read_nature(stream, frame, language);
    extras.ability_slug  = ocr_slug(stream, frame, ABILITY_BOX,   language, "Ability");
    extras.held_item_slug = ocr_slug(stream, frame, HELD_ITEM_BOX, language, "HeldItem");

    return extras;
}



}
}
}
