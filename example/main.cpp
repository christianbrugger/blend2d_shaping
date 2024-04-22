
#include <blend2d.h>

#include <iostream>

#include "blend2d_shaping.h"

auto render_text(BLContext& ctx, const char* filename) -> void {
    using namespace blend2d_shaping;

    const auto font_size = 45.25f;

    const auto face = create_face_from_file(filename);
    const auto font = create_font(face, font_size);

    ctx.save();
    ctx.translate(BLPoint {20, 70});

    const auto hb_text = HbShapedText {"Properly Shaped Text", font.hb_font, font_size};
    ctx.fillGlyphRun(BLPoint {}, font.bl_font, hb_text.glyph_run(), BLRgba32(0xFF000000));
    ctx.strokeRect(hb_text.bounding_rect(), BLRgba32(0xFFFF0000));

    ctx.restore();
}

auto render_image() {
    BLImage img(500, 140, BL_FORMAT_PRGB32);
    BLContext ctx(img);

    ctx.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx.fillAll();

    render_text(ctx, "fonts/NotoSans-Regular.ttf");

    ctx.end();
    img.writeToFile("output.png");
}

auto main() -> int {
    try {
        render_image();
    } catch (const std::runtime_error& exc) {
        std::cout << "Exception: " << exc.what() << std::endl;
        return 1;
    }
    return 0;
}
