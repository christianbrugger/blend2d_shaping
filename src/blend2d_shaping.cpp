#include "blend2d_shaping.h"

#include <hb.h>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <stdexcept>

namespace blend2d_shaping {

namespace {

/**
 * @brief Helpers taken from GSL (Guidelines Support Library):
 *
 *   https://github.com/microsoft/GSL
 *
 */

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

constexpr auto expects(auto condition) -> void {
    if (!!condition) {
        return;
    }
    std::terminate();
}

constexpr auto ensures(auto condition) -> void {
    if (!!condition) {
        return;
    }
    std::terminate();
}

//
// Harfbuzz RAII Wrapper
//

struct HbBlobDeleter {
    auto operator()(hb_blob_t *hb_blob) -> void;
};

struct HbFaceDeleter {
    auto operator()(hb_face_t *hb_face) -> void;
};

struct HbFontDeleter {
    auto operator()(hb_font_t *hb_font) -> void;
};

struct HbBufferDeleter {
    auto operator()(hb_buffer_t *hb_buffer) -> void;
};

using HbBlobPointer = std::unique_ptr<hb_blob_t, HbBlobDeleter>;
using HbFacePointer = std::unique_ptr<hb_face_t, HbFaceDeleter>;
using HbFontPointer = std::unique_ptr<hb_font_t, HbFontDeleter>;
using HbBufferPointer = std::unique_ptr<hb_buffer_t, HbBufferDeleter>;

auto HbBlobDeleter::operator()(hb_blob_t *hb_blob) -> void {
    hb_blob_destroy(hb_blob);
}

auto HbFaceDeleter::operator()(hb_face_t *hb_face) -> void {
    hb_face_destroy(hb_face);
}

auto HbFontDeleter::operator()(hb_font_t *hb_font) -> void {
    hb_font_destroy(hb_font);
}

auto HbBufferDeleter::operator()(hb_buffer_t *hb_buffer) -> void {
    hb_buffer_destroy(hb_buffer);
}

[[nodiscard]] auto create_hb_blob(std::span<const char> font_data) -> HbBlobPointer {
    const auto *data = font_data.data();
    const auto length = narrow<unsigned int>(font_data.size());
    const auto mode = hb_memory_mode_t::HB_MEMORY_MODE_DUPLICATE;

    void *user_data = nullptr;
    hb_destroy_func_t destroy = nullptr;

    auto blob = HbBlobPointer {
        hb_blob_create(data, length, mode, user_data, destroy),
    };

    expects(blob != nullptr);
    expects(hb_blob_get_length(blob.get()) == length);

    return blob;
}

[[nodiscard]] auto create_immutable_face() -> HbFacePointer {
    auto face = HbFacePointer {hb_face_reference(hb_face_get_empty())};
    hb_face_make_immutable(face.get());
    return face;
}

[[nodiscard]] auto create_immutable_face(std::span<const char> font_data,
                                         unsigned int font_index) -> HbFacePointer {
    const auto blob = create_hb_blob(font_data);

    auto face = HbFacePointer {hb_face_create(blob.get(), font_index)};
    hb_face_make_immutable(face.get());

    return face;
}

[[nodiscard]] auto create_immutable_font() -> HbFontPointer {
    auto font = HbFontPointer {hb_font_reference(hb_font_get_empty())};
    hb_font_make_immutable(font.get());
    return font;
}

[[nodiscard]] auto create_immutable_font(hb_face_t *hb_face) -> HbFontPointer {
    expects(hb_face);

    auto font = HbFontPointer {hb_font_create(hb_face)};
    hb_font_make_immutable(font.get());

    return font;
}

[[nodiscard]] auto shape_text(std::string_view text_utf8, hb_font_t *hb_font)
    -> HbBufferPointer {
    expects(hb_font != nullptr);

    auto buffer = HbBufferPointer {hb_buffer_create()};
    expects(buffer != nullptr);

    const auto text_length = narrow<int>(text_utf8.size());
    const auto item_offset = std::size_t {0};
    const auto item_length = text_length;
    hb_buffer_add_utf8(buffer.get(), text_utf8.data(), text_length, item_offset,
                       item_length);

    // set text properties
    hb_buffer_set_direction(buffer.get(), HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer.get(), HB_SCRIPT_LATIN);
    hb_buffer_set_language(buffer.get(), hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(buffer.get());

    // shape text
    const hb_feature_t *features = nullptr;
    const auto num_features = std::size_t {0};
    hb_shape(hb_font, buffer.get(), features, num_features);

    return buffer;
}

[[nodiscard]] auto get_glyph_infos(hb_buffer_t *hb_buffer) -> std::span<hb_glyph_info_t> {
    expects(hb_buffer != nullptr);

    const auto glyph_count = hb_buffer_get_length(hb_buffer);
    return std::span<hb_glyph_info_t>(hb_buffer_get_glyph_infos(hb_buffer, nullptr),
                                      glyph_count);
}

[[nodiscard]] auto get_hb_glyph_positions(hb_buffer_t *hb_buffer)
    -> std::span<hb_glyph_position_t> {
    expects(hb_buffer != nullptr);

    const auto glyph_count = hb_buffer_get_length(hb_buffer);
    return std::span<hb_glyph_position_t>(
        hb_buffer_get_glyph_positions(hb_buffer, nullptr), glyph_count);
}

[[nodiscard]] auto get_uint32_codepoints(hb_buffer_t *hb_buffer)
    -> std::vector<uint32_t> {
    expects(hb_buffer != nullptr);
    const auto glyph_infos = get_glyph_infos(hb_buffer);

    auto result = std::vector<uint32_t> {};
    result.reserve(glyph_infos.size());

    std::ranges::transform(
        glyph_infos, std::back_inserter(result),
        [](const hb_glyph_info_t &glyph_info) { return glyph_info.codepoint; });

    return result;
}

[[nodiscard]] auto get_bl_placements(hb_buffer_t *hb_buffer)
    -> std::vector<BLGlyphPlacement> {
    expects(hb_buffer != nullptr);
    const auto glyph_positions = get_hb_glyph_positions(hb_buffer);

    auto result = std::vector<BLGlyphPlacement> {};
    result.reserve(glyph_positions.size());

    std::ranges::transform(
        glyph_positions, std::back_inserter(result),
        [](const hb_glyph_position_t &position) {
            return BLGlyphPlacement {
                .placement = BLPointI {position.x_offset, position.y_offset},
                .advance = BLPointI {position.x_advance, position.y_advance},
            };
        });

    return result;
}

[[nodiscard]] auto calculate_bounding_rect(hb_buffer_t *hb_buffer, hb_font_t *hb_font,
                                           float font_size) -> BLBox {
    expects(hb_buffer != nullptr);
    expects(hb_font != nullptr);

    const auto glyph_infos = get_glyph_infos(hb_buffer);
    const auto glyph_positions = get_hb_glyph_positions(hb_buffer);

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

    const auto N = std::min(glyph_infos.size(), glyph_positions.size());
    for (std::size_t i = 0; i < N; ++i) {
        const auto &pos = glyph_positions[i];
        auto extents = hb_glyph_extents_t {};

        if (hb_font_get_glyph_extents(hb_font, glyph_infos[i].codepoint, &extents) != 0 &&
            extents.width != 0 && extents.height != 0) {
            const auto glyph_rect = BLBox {
                origin.x + pos.x_offset + extents.x_bearing,
                -(origin.y + pos.y_offset + extents.y_bearing),
                origin.x + pos.x_offset + extents.x_bearing + extents.width,
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

    return rect / scale * font_size;
}

}  // namespace

//
// Font Face
//

HbFontFace::HbFontFace() : face_ {create_immutable_face()} {
    expects(face_ != nullptr);
    ensures(hb_face_is_immutable(face_.get()));
}

HbFontFace::HbFontFace(std::span<const char> font_data, unsigned int font_index)
    : face_ {create_immutable_face(font_data, font_index)} {
    ensures(face_ != nullptr);
    ensures(hb_face_is_immutable(face_.get()));
}

namespace {

[[nodiscard]] auto to_char_span(std::span<const uint8_t> span) -> std::span<const char> {
    return std::span<const char>(reinterpret_cast<const char *>(span.data()),
                                 span.size());
}

}  // namespace

HbFontFace::HbFontFace(std::span<const uint8_t> font_data, unsigned int font_index)
    : HbFontFace {to_char_span(font_data), font_index} {}

auto HbFontFace::empty() const -> bool {
    return hb_face_get_glyph_count(hb_face()) == 0;
}

auto HbFontFace::hb_face() const noexcept -> hb_face_t * {
    expects(face_ != nullptr);
    ensures(hb_face_is_immutable(face_.get()));

    return face_.get();
}

//
// Font
//

HbFont::HbFont() : font_ {create_immutable_font()} {
    ensures(font_ != nullptr);
    ensures(hb_font_is_immutable(font_.get()));
}

HbFont::HbFont(const HbFontFace &face) : font_ {create_immutable_font(face.hb_face())} {
    ensures(font_ != nullptr);
    ensures(hb_font_is_immutable(font_.get()));
}

auto HbFont::empty() const -> bool {
    const auto *face = hb_font_get_face(hb_font());
    expects(face != nullptr);

    return hb_face_get_glyph_count(face) == 0;
}

auto HbFont::hb_font() const noexcept -> hb_font_t * {
    expects(font_ != nullptr);
    ensures(hb_font_is_immutable(font_.get()));

    return font_.get();
}

//
// Shaped Text
//

HbShapedText::HbShapedText(std::string_view text_utf8, const HbFont &font,
                           float font_size) {
    const auto buffer = shape_text(text_utf8, font.hb_font());

    codepoints_ = get_uint32_codepoints(buffer.get());
    placements_ = get_bl_placements(buffer.get());
    bounding_box_ = calculate_bounding_rect(buffer.get(), font.hb_font(), font_size);

    ensures(codepoints_.size() == placements_.size());
}

auto HbShapedText::empty() const -> bool {
    expects(codepoints_.size() == placements_.size());

    return codepoints_.empty();
}

auto HbShapedText::glyph_run() const noexcept -> BLGlyphRun {
    expects(codepoints_.size() == placements_.size());

    auto result = BLGlyphRun {};

    result.size = codepoints_.size();
    result.setGlyphData(codepoints_.data());
    result.setPlacementData(placements_.data());
    result.placementType = BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET;

    return result;
}

auto HbShapedText::bounding_box() const noexcept -> BLBox {
    return bounding_box_;
}

auto HbShapedText::bounding_rect() const noexcept -> BLRect {
    const auto box = bounding_box_;
    return BLRect {box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0};
}

//
// From File
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
        .hb_face = HbFontFace {buffer, face_index},
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
        .hb_font = HbFont {face.hb_face},
    };
}

}  // namespace blend2d_shaping
