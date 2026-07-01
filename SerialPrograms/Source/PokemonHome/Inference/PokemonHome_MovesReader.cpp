/*  Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/OCR/OCR_StringNormalization.h"
#include "CommonTools/OCR/OCR_TextMatcher.h"
#include "Pokemon/Inference/Pokemon_IvJudgeReader.h"
#include "PokemonHome_MovesReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Static dictionary matcher — one shared instance per process lifetime.
// ---------------------------------------------------------------------------
const OCR::SmallDictionaryMatcher& MOVES_READER(){
    static const OCR::SmallDictionaryMatcher reader("PokemonHome/PokemonMovesOCR.json");
    return reader;
}


// ---------------------------------------------------------------------------
// Constructor
//
// Crop boxes are PLACEHOLDER coordinates for a vertical 4-move stack on the
// right side of the HOME moves screen.  Calibrate on the capture rig (Task 7)
// before the program ships.  The pattern is {0.62, 0.22 + 0.12*i, 0.28, 0.06}
// for i in 0..3, expanded to four explicit literals below.
// ---------------------------------------------------------------------------
MovesReaderScope::MovesReaderScope(VideoOverlay& overlay, Language language)
    : m_language(language)
    , m_box_move1(overlay, {0.62, 0.22, 0.28, 0.06})  // PLACEHOLDER: slot 1
    , m_box_move2(overlay, {0.62, 0.34, 0.28, 0.06})  // PLACEHOLDER: slot 2
    , m_box_move3(overlay, {0.62, 0.46, 0.28, 0.06})  // PLACEHOLDER: slot 3
    , m_box_move4(overlay, {0.62, 0.58, 0.28, 0.06})  // PLACEHOLDER: slot 4
{}


// ---------------------------------------------------------------------------
// Private: read one slot
// ---------------------------------------------------------------------------
std::string MovesReaderScope::read_one(
    Logger& logger,
    const ImageViewRGB32& frame,
    const OverlayBoxScope& box,
    int slot_index
){
    ImageViewRGB32 image = extract_box_reference(frame, box);
    OCR::StringMatchResult result = MOVES_READER().match_substring_from_image_multifiltered(
        &logger,
        m_language,
        image,
        OCR::WHITE_TEXT_FILTERS(),
        Pokemon::IvJudgeReader::MAX_LOG10P,
        Pokemon::IvJudgeReader::MAX_LOG10P_SPREAD
    );
    result.clear_beyond_log10p(Pokemon::IvJudgeReader::MAX_LOG10P);
    if (result.results.size() != 1){
        logger.log(
            "MovesReader: move slot " + std::to_string(slot_index) +
            " OCR failed or ambiguous (candidates=" +
            std::to_string(result.results.size()) + ")",
            COLOR_YELLOW
        );
        return "";
    }
    const std::string& token = result.results.begin()->second.token;
    logger.log("MovesReader: slot " + std::to_string(slot_index) + " = " + token);
    return token;
}


// ---------------------------------------------------------------------------
// Public: read all four slots; skip empty/failed results
// ---------------------------------------------------------------------------
std::vector<std::string> MovesReaderScope::read(Logger& logger, const ImageViewRGB32& frame){
    std::vector<std::string> moves;
    moves.reserve(4);

    const OverlayBoxScope* boxes[4] = {&m_box_move1, &m_box_move2, &m_box_move3, &m_box_move4};
    for (int i = 0; i < 4; ++i){
        std::string token = read_one(logger, frame, *boxes[i], i + 1);
        if (!token.empty()){
            moves.push_back(std::move(token));
        }
    }
    return moves;
}


// ---------------------------------------------------------------------------
// Public: dump raw crop images for debugging
// ---------------------------------------------------------------------------
std::vector<ImageViewRGB32> MovesReaderScope::dump_images(const ImageViewRGB32& frame){
    std::vector<ImageViewRGB32> images;
    images.reserve(4);
    images.emplace_back(extract_box_reference(frame, m_box_move1));
    images.emplace_back(extract_box_reference(frame, m_box_move2));
    images.emplace_back(extract_box_reference(frame, m_box_move3));
    images.emplace_back(extract_box_reference(frame, m_box_move4));
    return images;
}


}
}
}
