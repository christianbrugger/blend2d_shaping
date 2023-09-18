#include "blend2d_shaping.h"

#include <blend2d.h>
#include <hb.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <numeric>
#include <ranges>
#include <stdexcept>

namespace blend2d_shaping {

namespace {

template <std::integral T, std::integral V>
auto narrow(const V val) -> T {
    if (std::in_range<T>(val)) {
        return static_cast<T>(val);
    }
    std::terminate();
}

template <std::integral T>
auto narrow(const double val) -> T {
    const auto cast = static_cast<T>(val);
    if (static_cast<double>(cast) == val) {
        return cast;
    }
    std::terminate();
}

}  // namespace

//
// Harfbuzz Blob
//

namespace {

class HBBlob {
   public:
    explicit HBBlob() : hb_blob {hb_blob_get_empty()} {}

    explicit HBBlob(std::span<const uint8_t> font_data)
        : hb_blob {hb_blob_create(reinterpret_cast<const char *>(font_data.data()),
                                  narrow<unsigned int>(font_data.size()),
                                  hb_memory_mode_t::HB_MEMORY_MODE_READONLY, nullptr,
                                  nullptr)} {
        if (!font_data.empty() && hb_blob == hb_blob_get_empty()) [[unlikely]] {
            throw std::runtime_error("Unable to load font_data in Harfbuzz");
        }
    }

    explicit HBBlob(const std::string &filename)
        : hb_blob {hb_blob_create_from_file(filename.c_str())} {
        if (!filename.empty() && hb_blob == hb_blob_get_empty()) [[unlikely]] {
            throw std::runtime_error("Unable to load font file in Harfbuzz");
        }
    }

    HBBlob(const HBBlob &) = delete;
    HBBlob(HBBlob &&) = delete;
    HBBlob &operator=(const HBBlob &) = delete;
    HBBlob &operator=(HBBlob &&) = delete;

    ~HBBlob() {
        hb_blob_destroy(hb_blob);
    }

   public:
    hb_blob_t *hb_blob;  //  not null
};

}  // namespace

//
// Harfbuzz Font Face
//

HBFontFace::HBFontFace() : hb_face_ {hb_face_get_empty()} {}

HBFontFace::HBFontFace(std::span<const uint8_t> font_data, uint32_t font_index)
    : hb_face_ {
          hb_face_create(HBBlob {font_data}.hb_blob, narrow<unsigned int>(font_index))} {
    hb_face_make_immutable(hb_face_);
}

HBFontFace::HBFontFace(const std::string &filename, uint32_t font_index)
    : hb_face_ {hb_face_create(HBBlob {filename.c_str()}.hb_blob,
                               narrow<unsigned int>(font_index))}

{
    hb_face_make_immutable(hb_face_);
}

HBFontFace::~HBFontFace() {
    hb_face_destroy(hb_face_);
}

HBFontFace::HBFontFace(const HBFontFace &other)
    : hb_face_ {hb_face_reference(other.hb_face())} {
    hb_face_make_immutable(hb_face_);
}

HBFontFace::HBFontFace(HBFontFace &&other) noexcept : HBFontFace() {
    using std::swap;
    swap(hb_face_, other.hb_face_);
}

HBFontFace &HBFontFace::operator=(const HBFontFace &other) {
    auto copy = HBFontFace {other};
    using std::swap;
    swap(hb_face_, copy.hb_face_);
    return *this;
}

HBFontFace &HBFontFace::operator=(HBFontFace &&other) noexcept {
    using std::swap;
    swap(hb_face_, other.hb_face_);
    return *this;
}

auto HBFontFace::hb_face() const noexcept -> hb_face_t * {
    if (hb_face_ == nullptr) {
        std::terminate();
    }
    return hb_face_;
}

//
// Harfbuzz Font
//

HBFont::HBFont() : hb_font_ {hb_font_get_empty()} {}

HBFont::HBFont(const HBFontFace &face, float font_size)
    : hb_font_ {hb_font_create(face.hb_face())}, font_size_ {font_size} {
    const auto rounded_font_size = narrow<unsigned int>(std::round(font_size));
    hb_font_set_ppem(hb_font_, rounded_font_size, rounded_font_size);
    hb_font_make_immutable(hb_font_);
}

HBFont::~HBFont() {
    hb_font_destroy(hb_font_);
}

HBFont::HBFont(const HBFont &other) : hb_font_ {hb_font_reference(other.hb_font())} {
    hb_font_make_immutable(hb_font_);
}

HBFont::HBFont(HBFont &&other) noexcept : HBFont() {
    using std::swap;
    swap(hb_font_, other.hb_font_);
}

HBFont &HBFont::operator=(HBFont &&other) noexcept {
    using std::swap;
    swap(hb_font_, other.hb_font_);
    return *this;
}

HBFont &HBFont::operator=(const HBFont &other) {
    auto copy = HBFont {other};
    using std::swap;
    swap(font_size_, copy.font_size_);
    return *this;
}

auto HBFont::font_size() const noexcept -> float {
    return font_size_;
}

auto HBFont::hb_font() const noexcept -> hb_font_t * {
    if (hb_font_ == nullptr) {
        std::terminate();
    }
    return hb_font_;
}

//
// Harfbuzz Buffer
//

namespace {

class HarfbuzzBuffer {
   public:
    explicit HarfbuzzBuffer() : hb_buffer {hb_buffer_create()} {}

    HarfbuzzBuffer(const HarfbuzzBuffer &) = delete;
    HarfbuzzBuffer(HarfbuzzBuffer &&) = delete;
    HarfbuzzBuffer &operator=(const HarfbuzzBuffer &) = delete;
    HarfbuzzBuffer &operator=(HarfbuzzBuffer &&) = delete;

    ~HarfbuzzBuffer() {
        hb_buffer_destroy(hb_buffer);
    }

   public:
    hb_buffer_t *hb_buffer;  //  not null
};

}  // namespace

//
// Harfbuzz Shaped Text
//

auto calculate_bounding_rect(std::span<const hb_glyph_info_t> glyph_info,
                             std::span<hb_glyph_position_t> glyph_positions,
                             const HBFont &font) -> BLBox {
    const auto hb_font = font.hb_font();

    auto scale = BLPointI {};
    hb_font_get_scale(hb_font, &scale.x, &scale.y);

    auto origin = BLPoint {};
    auto rect = BLBox {
        +std::numeric_limits<double>::infinity(),
        +std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    bool found = false;

    const auto N = std::min(glyph_info.size(), glyph_positions.size());
    for (std::size_t i = 0; i < N; ++i) {
        const auto &pos = glyph_positions[i];
        auto extents = hb_glyph_extents_t {};

        if (hb_font_get_glyph_extents(hb_font, glyph_info[i].codepoint, &extents) &&
            extents.width != 0 && extents.height != 0) {
            const auto glyph_rect = BLBox {
                +(origin.x + pos.x_offset + extents.x_bearing),
                -(origin.y + pos.y_offset + extents.y_bearing),
                +(origin.x + pos.x_offset + extents.x_bearing + extents.width),
                -(origin.y + pos.y_offset + extents.y_bearing + extents.height),
            };

            assert(glyph_rect.x0 <= glyph_rect.x1);
            assert(glyph_rect.y0 <= glyph_rect.y1);

            rect.x0 = std::min(rect.x0, glyph_rect.x0);
            rect.y0 = std::min(rect.y0, glyph_rect.y0);
            rect.x1 = std::max(rect.x1, glyph_rect.x1);
            rect.y1 = std::max(rect.y1, glyph_rect.y1);

            found = true;
        }

        origin.x += pos.x_advance;
        origin.y += pos.y_advance;
    }

    if (!found || scale.x == 0 || scale.y == 0) {
        return BLBox {};
    }

    return rect / scale * font.font_size();
}

HBShapedText::HBShapedText(std::string_view text_utf8, const HBFontFace &face,
                           float font_size)
    : HBShapedText {text_utf8, HBFont {face, font_size}} {}

HBShapedText::HBShapedText(std::string_view text_utf8, const HBFont &font) {
    const auto buffer = HarfbuzzBuffer {};
    const auto hb_buffer = buffer.hb_buffer;

    const auto text_length = narrow<int>(text_utf8.size());
    const auto item_offset = std::size_t {0};
    const auto item_length = text_length;
    hb_buffer_add_utf8(hb_buffer, text_utf8.data(), text_length, item_offset,
                       item_length);

    // set text properties
    hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(hb_buffer, hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(hb_buffer);

    // shape text
    const hb_feature_t *features = nullptr;
    const auto num_features = std::size_t {0};
    hb_shape(font.hb_font(), hb_buffer, features, num_features);

    // extract placement data
    const auto glyph_count = hb_buffer_get_length(hb_buffer);
    const auto glyph_infos = std::span<hb_glyph_info_t>(
        hb_buffer_get_glyph_infos(hb_buffer, nullptr), glyph_count);
    const auto glyph_positions = std::span<hb_glyph_position_t>(
        hb_buffer_get_glyph_positions(hb_buffer, nullptr), glyph_count);

    codepoints_.reserve(glyph_count);
    placements_.reserve(glyph_count);

    std::ranges::transform(
        glyph_infos, std::back_inserter(codepoints_),
        [](const hb_glyph_info_t &glyph_info) { return glyph_info.codepoint; });

    std::ranges::transform(
        glyph_positions, std::back_inserter(placements_),
        [](const hb_glyph_position_t &position) {
            return BLGlyphPlacement {
                .placement = BLPointI {position.x_offset, position.y_offset},
                .advance = BLPointI {position.x_advance, position.y_advance},
            };
        });

    bounding_box_ = calculate_bounding_rect(glyph_infos, glyph_positions, font);
}

auto HBShapedText::glyph_run() const noexcept -> BLGlyphRun {
    auto result = BLGlyphRun {};

    result.size = std::min(codepoints_.size(), placements_.size());
    result.setGlyphData(codepoints_.data());
    result.setPlacementData(placements_.data());
    result.placementType = BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET;

    return result;
}

auto HBShapedText::bounding_box() const noexcept -> BLBox {
    return bounding_box_;
}

auto HBShapedText::bounding_rect() const noexcept -> BLRect {
    const auto box = bounding_box_;
    return BLRect {box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0};
}

//
// Public Interface
//

auto create_face_from_file(const char *filename, uint32_t face_index) -> FontFace {
    BLArray<uint8_t> buffer;
    if (const auto result = BLFileSystem::readFile(filename, buffer);
        result != BL_SUCCESS) {
        throw std::runtime_error("Unable to load font file in BLFileSystem");
    }

    BLFontData data;
    if (const auto result = data.createFromData(buffer); result != BL_SUCCESS) {
        throw std::runtime_error("Unable create BLFontData");
    }

    BLFontFace face;
    if (const auto result = face.createFromData(data, face_index); result != BL_SUCCESS) {
        throw std::runtime_error("Unable create BLFontFace");
    }

    return FontFace {
        .bl_face = std::move(face),
        .hb_face = HBFontFace {buffer, face_index},
    };
}

auto create_font(const FontFace &face, float font_size) -> Font {
    BLFont font;

    if (const auto result = font.createFromFace(face.bl_face, font_size);
        result != BL_SUCCESS) {
        throw std::runtime_error("Unable create BLFont");
    }

    return Font {
        .bl_font = std::move(font),
        .hb_font = HBFont {face.hb_face, font_size},
    };
}

}  // namespace blend2d_shaping
