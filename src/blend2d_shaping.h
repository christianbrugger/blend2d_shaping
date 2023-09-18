#ifndef BLEND2D_SHAPING_BLEND2D_SHAPING_H
#define BLEND2D_SHAPING_BLEND2D_SHAPING_H

#include <blend2d.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

struct hb_blob_t;
struct hb_face_t;
struct hb_font_t;

namespace blend2d_shaping {

class HBFontFace final {
   public:
    [[nodiscard]] explicit HBFontFace();
    [[nodiscard]] explicit HBFontFace(std::span<const uint8_t> font_data,
                                      uint32_t font_index = 0);
    [[nodiscard]] explicit HBFontFace(const std::string &filename,
                                      uint32_t font_index = 0);
    ~HBFontFace();

    HBFontFace(const HBFontFace &);
    HBFontFace(HBFontFace &&) noexcept;
    HBFontFace &operator=(const HBFontFace &);
    HBFontFace &operator=(HBFontFace &&) noexcept;

    [[nodiscard]] auto hb_face() const noexcept -> hb_face_t *;

   private:
    hb_face_t *hb_face_;
};

class HBFont final {
   public:
    [[nodiscard]] explicit HBFont();
    [[nodiscard]] explicit HBFont(const HBFontFace &face, float font_size);
    ~HBFont();

    HBFont(const HBFont &);
    HBFont(HBFont &&) noexcept;
    HBFont &operator=(const HBFont &);
    HBFont &operator=(HBFont &&) noexcept;

    [[nodiscard]] auto font_size() const noexcept -> float;
    [[nodiscard]] auto hb_font() const noexcept -> hb_font_t *;

   private:
    hb_font_t *hb_font_;  // TODO not_null
    float font_size_ {};
};

class HBShapedText {
   public:
    [[nodiscard]] explicit HBShapedText() = default;
    [[nodiscard]] explicit HBShapedText(std::string_view text_utf8,
                                        const HBFontFace &face, float font_size);
    [[nodiscard]] explicit HBShapedText(std::string_view text_utf8, const HBFont &font);

    [[nodiscard]] auto operator==(const HBShapedText &other) const -> bool = default;

    // glyph run of the shaped text
    [[nodiscard]] auto glyph_run() const noexcept -> BLGlyphRun;

    /// bounding box of the shaped text relative to the baseline
    [[nodiscard]] auto bounding_box() const noexcept -> BLBox;
    /// rect of the shaped text relative to the baseline
    [[nodiscard]] auto bounding_rect() const noexcept -> BLRect;

   private:
    std::vector<uint32_t> codepoints_ {};
    std::vector<BLGlyphPlacement> placements_ {};

    BLBox bounding_box_ {};
};

struct FontFace {
    BLFontFace bl_face {};
    HBFontFace hb_face {};
};

struct Font {
    BLFont bl_font {};
    HBFont hb_font {};
};

[[nodiscard]] auto create_face_from_file(const char *filename, uint32_t face_index = 0)
    -> FontFace;

[[nodiscard]] auto create_font(const FontFace &face, float font_size) -> Font;

}  // namespace blend2d_shaping

#endif