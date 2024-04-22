#ifndef BLEND2D_SHAPING_BLEND2D_SHAPING_H
#define BLEND2D_SHAPING_BLEND2D_SHAPING_H

#include <blend2d.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct hb_face_t;
struct hb_font_t;

namespace blend2d_shaping {

class HbFontFace final {
   public:
    explicit HbFontFace();
    explicit HbFontFace(std::span<const char> font_data, unsigned int font_index = 0);
    explicit HbFontFace(std::span<const uint8_t> font_data, unsigned int font_index = 0);

    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto hb_face() const noexcept -> hb_face_t *;

   private:
    // immutable preserves whole parts relationship
    std::shared_ptr<hb_face_t> face_;
};

static_assert(std::semiregular<HbFontFace>);

class HbFont final {
   public:
    explicit HbFont();
    explicit HbFont(const HbFontFace &face);

    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto hb_font() const noexcept -> hb_font_t *;

   private:
    // immutable preserves whole parts relationship
    std::shared_ptr<hb_font_t> font_;
};

static_assert(std::semiregular<HbFont>);

class HbShapedText {
   public:
    explicit HbShapedText() = default;
    explicit HbShapedText(std::string_view text_utf8, const HbFont &font,
                          float font_size);

    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto operator==(const HbShapedText &other) const -> bool = default;

    // glyph run of the shaped text
    [[nodiscard]] auto glyph_run() const noexcept -> BLGlyphRun;
    // bounding box of the shaped text relative to the baseline
    [[nodiscard]] auto bounding_box() const noexcept -> BLBox;
    // rect of the shaped text relative to the baseline
    [[nodiscard]] auto bounding_rect() const noexcept -> BLRect;

   private:
    std::vector<uint32_t> codepoints_ {};
    std::vector<BLGlyphPlacement> placements_ {};
    BLBox bounding_box_ {};
};

static_assert(std::regular<HbShapedText>);

struct FontFace {
    BLFontFace bl_face {};
    HbFontFace hb_face {};
};

struct Font {
    BLFont bl_font {};
    HbFont hb_font {};
};

[[nodiscard]] auto create_face_from_file(const char *filename, uint32_t face_index = 0)
    -> FontFace;

[[nodiscard]] auto create_font(const FontFace &face, float font_size) -> Font;

}  // namespace blend2d_shaping

#endif