
#include <blend2d.h>

#include "blend2d_shaping.h"

auto render_text(BLContext& ctx, const char* filename) -> void {
    using namespace blend2d_shaping;

    const auto face = create_face_from_file(filename);
    const auto font = create_font(face, 45.25);

    ctx.save();
    ctx.translate(BLPoint {20, 70});

    const auto hb_text = HBShapedText {"Properly Shaped Text", font.hb_font};
    ctx.fillGlyphRun(BLPoint {}, font.bl_font, hb_text.glyph_run(), BLRgba32(0xFF000000));
    ctx.strokeRect(hb_text.bounding_rect(), BLRgba32(0xFFFF0000));

    ctx.restore();
}

auto main() -> int {
    BLImage img(500, 140, BL_FORMAT_PRGB32);
    BLContext ctx(img);

    ctx.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx.fillAll();

    render_text(ctx, "fonts/NotoSans-Regular.ttf");

    ctx.end();
    img.writeToFile("output.png");

    return 0;
}
