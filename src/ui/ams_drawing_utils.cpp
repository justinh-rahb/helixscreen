// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ams_drawing_utils.h"

#include <algorithm>

namespace ams_draw {

// ============================================================================
// Color Utilities
// ============================================================================

lv_color_t lighten_color(lv_color_t c, uint8_t amount) {
    return lv_color_make(std::min(255, c.red + amount), std::min(255, c.green + amount),
                         std::min(255, c.blue + amount));
}

lv_color_t darken_color(lv_color_t c, uint8_t amount) {
    return lv_color_make(c.red > amount ? c.red - amount : 0,
                         c.green > amount ? c.green - amount : 0,
                         c.blue > amount ? c.blue - amount : 0);
}

lv_color_t blend_color(lv_color_t c1, lv_color_t c2, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    return lv_color_make(static_cast<uint8_t>(c1.red + (c2.red - c1.red) * factor),
                         static_cast<uint8_t>(c1.green + (c2.green - c1.green) * factor),
                         static_cast<uint8_t>(c1.blue + (c2.blue - c1.blue) * factor));
}

} // namespace ams_draw
