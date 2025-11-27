# LVGL 9 XML Attributes - Complete Reference

**Comprehensive attribute documentation extracted from LVGL 9.3 source code**

**Last Updated:** 2025-01-14
**LVGL Version:** 9.3
**Source:** lvgl/src/others/xml/parsers/*.c

---

## Table of Contents

1. [Introduction](#introduction)
2. [Base Object Attributes](#base-object-attributes-lv_obj)
3. [Style Attributes](#style-attributes-style_-prefix)
4. [Widget-Specific Attributes](#widget-specific-attributes)
5. [Enum Values Reference](#enum-values-reference)
6. [Critical Gotchas](#critical-gotchas)
7. [Source Code References](#source-code-references)

---

## Introduction

### How XML Attributes Work

LVGL 9's XML system maps XML attributes directly to C API function calls. Each widget type has an XML parser that translates attributes into the corresponding C function:

```xml
<!-- XML Attribute -->
<lv_label width="100" text="Hello" style_text_color="0xff0000"/>

<!-- Translates to C API calls -->
lv_obj_set_width(label, 100);
lv_label_set_text(label, "Hello");
lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), 0);
```

### Attribute Naming Conventions

1. **Base attributes**: Direct widget properties (e.g., `width`, `text`, `align`)
2. **Style attributes**: Prefixed with `style_` (e.g., `style_bg_color`, `style_text_font`)
3. **Flag attributes**: Boolean flags that control widget behavior (e.g., `hidden`, `clickable`)
4. **State attributes**: Widget states (e.g., `checked`, `focused`, `disabled`)
5. **Binding attributes**: Reactive data binding (e.g., `bind_text`, `bind_value`, `bind_flag_if_eq`)

### Important Rules

- **No abbreviations**: Must use full words (e.g., `style_image_recolor` NOT `style_img_recolor`)
- **Case sensitive**: Attribute names are lowercase with underscores
- **Silent failures**: Unknown attributes are ignored without warnings
- **Inheritance**: All widgets inherit base `lv_obj` attributes

---

## Base Object Attributes (lv_obj)

All LVGL widgets inherit these attributes from `lv_obj`. These attributes are processed by `lv_xml_obj_parser.c`.

### Layout & Positioning

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `name` | string | Widget name for lookup with `lv_obj_find_by_name()` | `name="my_button"` |
| `x` | size | Horizontal position (px or %) | `x="20"` or `x="50%"` |
| `y` | size | Vertical position (px or %) | `y="30"` or `y="10%"` |
| `width` | size | Widget width (px, %, or "content") | `width="100"` or `width="50%"` or `width="content"` |
| `height` | size | Widget height (px, %, or "content") | `height="50"` or `height="100%"` or `height="content"` |
| `align` | enum | Alignment relative to parent | `align="center"` or `align="top_left"` |
| `ext_click_area` | int | Extended clickable area (pixels) | `ext_click_area="10"` |

**Size values:**
- Pixel values: `"100"`, `"250"`
- Percentages: `"50%"`, `"100%"`
- Content-based: `"content"` (auto-size based on content) **⚠️ Often unreliable, use explicit sizes**

**Align values:** `top_left`, `top_mid`, `top_right`, `bottom_left`, `bottom_mid`, `bottom_right`, `left_mid`, `right_mid`, `center`

### Flex Layout

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `flex_flow` | enum | Flex direction | `flex_flow="row"` or `flex_flow="column"` |
| `flex_grow` | int | Flex grow factor (0 = fixed, >0 = proportional growth) | `flex_grow="1"` |
| `flex_in_new_track` | bool | Start widget in new flex track (line wrap) | `flex_in_new_track="true"` |

**Flex flow values:** `column`, `column_reverse`, `column_wrap`, `column_wrap_reverse`, `row`, `row_reverse`, `row_wrap`, `row_wrap_reverse`

**⚠️ IMPORTANT:** The attribute `flex_align` does NOT exist. Use `style_flex_main_place` and `style_flex_cross_place` instead (see Style Attributes section).

### Flags

All flag attributes are boolean (`true` or `false`). Setting a flag to `true` enables it, `false` disables it.

| Flag Attribute | Description |
|----------------|-------------|
| `hidden` | Widget is hidden (not drawn) |
| `clickable` | Widget can receive click events |
| `click_focusable` | Widget can be focused by clicking |
| `checkable` | Widget can be checked/unchecked |
| `scrollable` | Widget content can be scrolled |
| `scroll_elastic` | Scrolling has elastic (bounce) effect |
| `scroll_momentum` | Scrolling continues with momentum after release |
| `scroll_one` | Only one child can be scrolled at a time |
| `scroll_chain_hor` | Horizontal scroll chains to parent when at edge |
| `scroll_chain_ver` | Vertical scroll chains to parent when at edge |
| `scroll_chain` | Both horizontal and vertical scroll chaining |
| `scroll_on_focus` | Scroll to widget when it gets focus |
| `scroll_with_arrow` | Can scroll with arrow keys |
| `snappable` | Widget snaps to scroll positions |
| `press_lock` | Continue to send events even if press slides out of widget |
| `event_bubble` | Events bubble up to parent |
| `gesture_bubble` | Gestures bubble up to parent |
| `adv_hittest` | Advanced hit-testing (only drawn areas are clickable) |
| `ignore_layout` | Widget is ignored by parent layout |
| `floating` | Widget does not scroll with parent |
| `send_draw_task_events` | Send draw task events |
| `overflow_visible` | Child widgets can overflow parent bounds |

**Example:**
```xml
<lv_obj hidden="false" clickable="true" scrollable="false"/>
```

### State Attributes

State attributes control widget visual state. All are boolean.

| State Attribute | Description |
|-----------------|-------------|
| `checked` | Widget is in checked state |
| `focused` | Widget has focus |
| `focus_key` | Widget has focus from keyboard |
| `edited` | Widget is being edited |
| `hovered` | Widget is being hovered (mouse/pointer) |
| `pressed` | Widget is being pressed |
| `scrolled` | Widget is being scrolled |
| `disabled` | Widget is disabled (grayed out, non-interactive) |

**Example:**
```xml
<lv_checkbox checked="true" disabled="false"/>
```

### Data Binding Attributes

#### Simple Binding

| Attribute | Type | Description |
|-----------|------|-------------|
| `bind_checked` | subject | Bind checkbox state to integer subject | `bind_checked="is_enabled"` |

#### Conditional Flag Binding

Bind widget flags based on subject value comparisons:

| Attribute | Description | Syntax |
|-----------|-------------|--------|
| `bind_flag_if_eq` | Set flag if subject equals value | `bind_flag_if_eq="subject_name flag_name ref_value"` |
| `bind_flag_if_not_eq` | Set flag if subject not equal to value | `bind_flag_if_not_eq="subject_name flag_name ref_value"` |
| `bind_flag_if_gt` | Set flag if subject greater than value | `bind_flag_if_gt="subject_name flag_name ref_value"` |
| `bind_flag_if_ge` | Set flag if subject >= value | `bind_flag_if_ge="subject_name flag_name ref_value"` |
| `bind_flag_if_lt` | Set flag if subject less than value | `bind_flag_if_lt="subject_name flag_name ref_value"` |
| `bind_flag_if_le` | Set flag if subject <= value | `bind_flag_if_le="subject_name flag_name ref_value"` |

**Example:**
```xml
<!-- Hide panel when active_panel subject != 0 -->
<lv_obj bind_flag_if_not_eq="active_panel hidden 0"/>
```

#### Conditional State Binding

Same syntax as flag binding, but for widget states:

| Attribute | Description |
|-----------|-------------|
| `bind_state_if_eq` | Set state if subject equals value |
| `bind_state_if_not_eq` | Set state if subject not equal to value |
| `bind_state_if_gt` | Set state if subject greater than value |
| `bind_state_if_ge` | Set state if subject >= value |
| `bind_state_if_lt` | Set state if subject less than value |
| `bind_state_if_le` | Set state if subject <= value |

**Example:**
```xml
<!-- Set disabled state when power subject == 0 -->
<lv_button bind_state_if_eq="power disabled 0"/>
```

---

## Style Attributes (style_* prefix)

Style attributes are prefixed with `style_` and map to `lv_obj_set_style_*()` functions. All styles support state selectors (see selector suffix section below).

### Size & Dimensions

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_width` | size | Width override |
| `style_min_width` | size | Minimum width |
| `style_max_width` | size | Maximum width |
| `style_height` | size | Height override |
| `style_min_height` | size | Minimum height |
| `style_max_height` | size | Maximum height |
| `style_length` | size | Generic length (widget-specific) |
| `style_radius` | size | Corner radius |

### Padding

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_pad_left` | int | Left padding |
| `style_pad_right` | int | Right padding |
| `style_pad_top` | int | Top padding |
| `style_pad_bottom` | int | Bottom padding |
| `style_pad_hor` | int | Horizontal padding (left + right) |
| `style_pad_ver` | int | Vertical padding (top + bottom) |
| `style_pad_all` | int | All sides padding |
| `style_pad_row` | int | Padding between rows (flex/grid) |
| `style_pad_column` | int | Padding between columns (flex/grid) |
| `style_pad_gap` | int | Gap between flex/grid items |
| `style_pad_radial` | int | Radial padding (for arcs) |

### Margin

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_margin_left` | int | Left margin |
| `style_margin_right` | int | Right margin |
| `style_margin_top` | int | Top margin |
| `style_margin_bottom` | int | Bottom margin |
| `style_margin_hor` | int | Horizontal margin (left + right) |
| `style_margin_ver` | int | Vertical margin (top + bottom) |
| `style_margin_all` | int | All sides margin |

### Background

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_bg_color` | color | Background color |
| `style_bg_opa` | opacity | Background opacity |
| `style_bg_grad_dir` | enum | Gradient direction (`none`, `hor`, `ver`) |
| `style_bg_grad_color` | color | Gradient end color |
| `style_bg_main_stop` | int | Gradient main color stop position (0-255) |
| `style_bg_grad_stop` | int | Gradient end color stop position (0-255) |
| `style_bg_grad` | gradient | Gradient descriptor reference |
| `style_bg_image_src` | image | Background image source |
| `style_bg_image_tiled` | bool | Tile background image |
| `style_bg_image_recolor` | color | Background image recolor tint |
| `style_bg_image_recolor_opa` | opacity | Background image recolor opacity |

**Color format:** Hex color values like `"0xff0000"` (red), `"0x00ff00"` (green), `"0x0000ff"` (blue)

**Opacity format:** `0-255` or percentage `"0%"` to `"100%"`

### Border

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_border_color` | color | Border color |
| `style_border_width` | int | Border width |
| `style_border_opa` | opacity | Border opacity |
| `style_border_side` | enum | Which sides have border |
| `style_border_post` | bool | Draw border after children |

**Border side values:** `none`, `top`, `bottom`, `left`, `right`, `full`

### Outline

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_outline_color` | color | Outline color |
| `style_outline_width` | int | Outline width |
| `style_outline_opa` | opacity | Outline opacity |
| `style_outline_pad` | int | Space between widget and outline |

### Shadow

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_shadow_width` | int | Shadow blur width |
| `style_shadow_color` | color | Shadow color |
| `style_shadow_offset_x` | int | Shadow horizontal offset |
| `style_shadow_offset_y` | int | Shadow vertical offset |
| `style_shadow_spread` | int | Shadow spread amount |
| `style_shadow_opa` | opacity | Shadow opacity |

### Text

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_text_color` | color | Text color |
| `style_text_font` | font | Font reference |
| `style_text_opa` | opacity | Text opacity |
| `style_text_align` | enum | Text alignment |
| `style_text_letter_space` | int | Letter spacing |
| `style_text_line_space` | int | Line spacing |
| `style_text_decor` | enum | Text decoration |

**Text align values:** `left`, `right`, `center`, `auto`

**Text decor values:** `none`, `underline`, `strikethrough`

### Image

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_image_opa` | opacity | Image opacity |
| `style_image_recolor` | color | Image recolor tint |
| `style_image_recolor_opa` | opacity | Image recolor opacity |

**⚠️ CRITICAL:** Must use `image` not `img`. `style_img_recolor` will NOT work.

### Line

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_line_color` | color | Line color |
| `style_line_opa` | opacity | Line opacity |
| `style_line_width` | int | Line width |
| `style_line_dash_width` | int | Dash width for dashed lines |
| `style_line_dash_gap` | int | Gap between dashes |
| `style_line_rounded` | bool | Round line ends |

### Arc

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_arc_color` | color | Arc color |
| `style_arc_opa` | opacity | Arc opacity |
| `style_arc_width` | int | Arc width |
| `style_arc_rounded` | bool | Round arc ends |
| `style_arc_image_src` | image | Arc image source |

### Layout & Positioning

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_base_dir` | enum | Base text direction |
| `style_clip_corner` | bool | Clip content at rounded corners |
| `style_layout` | enum | Layout type |
| `style_align_self` | enum | Alignment within parent flex/grid |

**Base dir values:** `auto`, `ltr`, `rtl`

**Layout values:** `none`, `flex`, `grid`

### Flex Layout Styles

**⚠️ IMPORTANT:** Use these instead of the non-existent `flex_align` attribute!

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_flex_flow` | enum | Flex direction (same as `flex_flow` attribute) |
| `style_flex_grow` | int | Flex grow factor |
| `style_flex_main_place` | enum | Main axis alignment |
| `style_flex_cross_place` | enum | Cross axis alignment |
| `style_flex_track_place` | enum | Track placement in wrap mode |

**Flex align values:** `start`, `end`, `center`, `space_between`, `space_around`, `space_evenly`

**Example:**
```xml
<!-- Horizontal flex with centered items -->
<lv_obj flex_flow="row"
        style_flex_main_place="center"
        style_flex_cross_place="center">
    <!-- Children will be centered both horizontally and vertically -->
</lv_obj>
```

### Grid Layout Styles

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_grid_column_align` | enum | Column alignment |
| `style_grid_row_align` | enum | Row alignment |
| `style_grid_cell_column_pos` | int | Cell column position |
| `style_grid_cell_column_span` | int | Cell column span |
| `style_grid_cell_x_align` | enum | Cell horizontal alignment |
| `style_grid_cell_row_pos` | int | Cell row position |
| `style_grid_cell_row_span` | int | Cell row span |
| `style_grid_cell_y_align` | enum | Cell vertical alignment |

**Grid align values:** `start`, `end`, `center`, `stretch`, `space_between`, `space_around`, `space_evenly`

### Opacity & Effects

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_opa` | opacity | Overall widget opacity |
| `style_opa_layered` | opacity | Layered opacity |
| `style_color_filter_opa` | opacity | Color filter opacity |
| `style_anim_duration` | int | Animation duration (ms) |
| `style_blend_mode` | enum | Blend mode |
| `style_recolor` | color | General recolor tint |
| `style_recolor_opa` | opacity | General recolor opacity |

**Blend mode values:** `normal`, `additive`, `subtractive`, `multiply`, `difference`

### Transforms

| Attribute | Type | Description |
|-----------|------|-------------|
| `style_transform_width` | int | Transform width offset |
| `style_transform_height` | int | Transform height offset |
| `style_translate_x` | int | Horizontal translation |
| `style_translate_y` | int | Vertical translation |
| `style_translate_radial` | int | Radial translation |
| `style_transform_scale_x` | int | Horizontal scale (256 = 100%) |
| `style_transform_scale_y` | int | Vertical scale (256 = 100%) |
| `style_transform_rotation` | int | Rotation angle (0.1 degree units) |
| `style_transform_pivot_x` | int | Rotation pivot X |
| `style_transform_pivot_y` | int | Rotation pivot Y |
| `style_transform_skew_x` | int | Horizontal skew |
| `style_bitmap_mask_src` | image | Bitmap mask source |
| `style_rotary_sensitivity` | int | Rotary encoder sensitivity |

---

## Widget-Specific Attributes

### lv_label

**Parser:** `lv_xml_label_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `text` | string | Label text content | `text="Hello World"` |
| `long_mode` | enum | Text overflow behavior | `long_mode="wrap"` |
| `bind_text` | subject [format] | Bind to string subject with optional format | `bind_text="temp_value '%.1f°C'"` |

**Long mode values:** `wrap`, `scroll`, `scroll_circular`, `dots`, `clip`

**Bind text format:**
```xml
<!-- Simple binding (no format) -->
<lv_label bind_text="status_message"/>

<!-- With printf-style format string (use single quotes) -->
<lv_label bind_text="temperature '%.1f°C'"/>
<lv_label bind_text="count '%d items'"/>
```

### lv_image

**Parser:** `lv_xml_image_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `src` | image | Image source | `src="A:path/to/image.png"` or `src="my_image"` |
| `inner_align` | enum | Alignment within widget bounds | `inner_align="center"` |
| `rotation` | int | Rotation angle (0.1° units, 0-3600) | `rotation="450"` (45°) |
| `scale_x` | int | Horizontal scale (256 = 100%) | `scale_x="512"` (200%) |
| `scale_y` | int | Vertical scale (256 = 100%) | `scale_y="128"` (50%) |
| `pivot` | int int | Rotation pivot point (x y) | `pivot="50 50"` |

**Inner align values:** `top_left`, `top_mid`, `top_right`, `bottom_left`, `bottom_mid`, `bottom_right`, `left_mid`, `right_mid`, `center`, `stretch`, `tile`

**⚠️ CRITICAL:** The attribute `zoom` does NOT exist in LVGL 9. Use `scale_x` and `scale_y` instead.

**Scale values:**
- `256` = 100% (original size)
- `128` = 50%
- `512` = 200%

**Example:**
```xml
<!-- Scale image to 150% -->
<lv_image src="icon" scale_x="384" scale_y="384"/>

<!-- Rotate 90 degrees -->
<lv_image src="arrow" rotation="900"/>
```

### lv_button

**Parser:** `lv_xml_button_parser.c`

No widget-specific attributes. Inherits all attributes from `lv_obj`.

### lv_slider

**Parser:** `lv_xml_slider_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `value` | int [animated] | Current value (and optional animation flag) | `value="50 true"` |
| `start_value` | int [animated] | Start value for range mode | `start_value="20 false"` |
| `bind_value` | subject | Bind to integer subject | `bind_value="volume"` |
| `orientation` | enum | Slider orientation | `orientation="horizontal"` |
| `mode` | enum | Slider mode | `mode="normal"` |
| `range_min` | int | Minimum range value | `range_min="0"` |
| `range_max` | int | Maximum range value | `range_max="100"` |
| `range` | int int | Set both min and max | `range="0 100"` |

**Orientation values:** `auto`, `horizontal`, `vertical`

**Mode values:** `normal`, `range`, `symmetrical`

### lv_bar

**Parser:** `lv_xml_bar_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `value` | int [animated] | Current value | `value="75 true"` |
| `start_value` | int [animated] | Start value for range mode | `start_value="25"` |
| `orientation` | enum | Bar orientation | `orientation="horizontal"` |
| `mode` | enum | Bar mode | `mode="normal"` |
| `range` | int int | Min and max range | `range="0 100"` |

**Orientation values:** `auto`, `horizontal`, `vertical`

**Mode values:** `normal`, `range`, `symmetrical`

### lv_arc

**Parser:** `lv_xml_arc_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `angles` | int int | Start and end angles | `angles="0 270"` |
| `bg_angles` | int int | Background arc angles | `bg_angles="0 360"` |
| `range` | int int | Value range | `range="0 100"` |
| `value` | int | Current value | `value="50"` |
| `mode` | enum | Arc mode | `mode="normal"` |
| `bind_value` | subject | Bind to integer subject | `bind_value="progress"` |

**Mode values:** `normal`, `symmetrical`, `reverse`

### lv_textarea

**Parser:** `lv_xml_textarea_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `text` | string | Initial text content | `text="Type here..."` |
| `placeholder` | string | Placeholder text | `placeholder="Enter name"` |
| `one_line` | bool | Single line mode | `one_line="true"` |
| `password_mode` | bool | Password mode (hide characters) | `password_mode="true"` |
| `password_show_time` | int | Time to show password chars (ms) | `password_show_time="1000"` |
| `text_selection` | bool | Enable text selection | `text_selection="true"` |
| `cursor_pos` | int | Cursor position | `cursor_pos="0"` |

### lv_checkbox

**Parser:** `lv_xml_checkbox_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `text` | string | Checkbox label text | `text="Enable feature"` |

### lv_dropdown

**Parser:** `lv_xml_dropdown_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `options` | string | Newline-separated options | `options="Option 1\nOption 2\nOption 3"` |
| `text` | string | Dropdown text override | `text="Select..."` |
| `selected` | int | Selected option index | `selected="0"` |
| `symbol` | image | Dropdown symbol/icon | `symbol="dropdown_arrow"` |
| `bind_value` | subject | Bind to integer subject | `bind_value="selected_option"` |

### lv_roller

**Parser:** `lv_xml_roller_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `options` | string mode | Options string with mode | `options="'1\n2\n3\n4\n5' infinite"` |
| `selected` | int [animated] | Selected option index | `selected="2 true"` |
| `visible_row_count` | int | Number of visible rows | `visible_row_count="5"` |
| `bind_value` | subject | Bind to integer subject | `bind_value="selected_row"` |

**Mode values:** `normal`, `infinite`

**Note:** Options format requires quotes around the option list, followed by mode.

### lv_chart

**Parser:** `lv_xml_chart_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `point_count` | int | Number of data points | `point_count="10"` |
| `type` | enum | Chart type | `type="line"` |
| `update_mode` | enum | How chart updates when full | `update_mode="shift"` |
| `div_line_count` | int int | Division line count (hor ver) | `div_line_count="5 5"` |

**Type values:** `none`, `line`, `bar`, `scatter`

**Update mode values:** `shift`, `circular`

**Child elements:**
- `<chart_series>` - Add data series
  - Attributes: `color`, `axis` (primary_y, primary_x, secondary_y, secondary_x), `values`
- `<chart_cursor>` - Add cursor
  - Attributes: `color`, `dir`, `pos_x`, `pos_y`
- `<chart_axis>` - Configure axis
  - Attributes: `axis`, `range`

### lv_scale

**Parser:** `lv_xml_scale_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `mode` | enum | Scale orientation | `mode="horizontal_bottom"` |
| `total_tick_count` | int | Total number of ticks | `total_tick_count="21"` |
| `major_tick_every` | int | Major tick frequency | `major_tick_every="5"` |
| `label_show` | bool | Show labels | `label_show="true"` |
| `post_draw` | bool | Draw after children | `post_draw="false"` |
| `draw_ticks_on_top` | bool | Draw ticks on top | `draw_ticks_on_top="true"` |
| `range` | int int | Value range | `range="0 100"` |
| `angle_range` | int | Angle range for round scales | `angle_range="270"` |
| `rotation` | int | Rotation angle | `rotation="0"` |

**Mode values:** `horizontal_top`, `horizontal_bottom`, `vertical_left`, `vertical_right`, `round_inner`, `round_outer`

### lv_table

**Parser:** `lv_xml_table_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `column_count` | int | Number of columns | `column_count="3"` |
| `row_count` | int | Number of rows | `row_count="5"` |
| `selected_cell` | int int | Selected cell (row col) | `selected_cell="0 0"` |

**Child elements:**
- `<table_column>` - Configure column
  - Attributes: `column`, `width`
- `<table_cell>` - Set cell content
  - Attributes: `row`, `column`, `value`, `ctrl`
  - **Ctrl values:** `none`, `merge_right`, `text_crop`, `custom_1`, `custom_2`, `custom_3`, `custom_4`

### lv_buttonmatrix

**Parser:** `lv_xml_buttonmatrix_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `map` | string | Button labels (quoted, space-separated) | `map="'Btn1' 'Btn2' '\\n' 'Btn3'"` |
| `ctrl_map` | string | Control flags per button | See example below |
| `selected_button` | int | Selected button index | `selected_button="0"` |
| `one_checked` | bool | Only one button can be checked | `one_checked="true"` |

**Ctrl map values** (can combine with `|`):
`none`, `hidden`, `no_repeat`, `disabled`, `checkable`, `checked`, `click_trig`, `popover`, `recolor`, `width_1` through `width_15`, `custom_1`, `custom_2`

**Example:**
```xml
<lv_buttonmatrix
    map="'1' '2' '3' '\n' '4' '5' '6'"
    ctrl_map="width_2 width_1 width_1 width_1|checkable width_1 width_1"/>
```

### lv_spangroup

**Parser:** `lv_xml_spangroup_parser.c`

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `overflow` | enum | Text overflow mode | `overflow="clip"` |
| `max_lines` | int | Maximum number of lines | `max_lines="3"` |
| `indent` | int | First line indentation | `indent="20"` |

**Overflow values:** `clip`, `ellipsis`

**Child elements:**
- `<span>` - Add text span
  - Attributes: `text`, `style`

---

## Enum Values Reference

Extracted from `lv_xml_base_types.c`.

### lv_align_t (align, inner_align)

- `top_left`
- `top_mid`
- `top_right`
- `bottom_left`
- `bottom_mid`
- `bottom_right`
- `left_mid`
- `right_mid`
- `center`

### lv_state_t (state attributes)

- `default`
- `pressed`
- `checked`
- `hovered`
- `scrolled`
- `disabled`
- `focused`
- `focus_key`
- `edited`
- `user_1`
- `user_2`
- `user_3`
- `user_4`

### lv_flex_flow_t (flex_flow, style_flex_flow)

- `column`
- `column_reverse`
- `column_wrap`
- `column_wrap_reverse`
- `row`
- `row_reverse`
- `row_wrap`
- `row_wrap_reverse`

### lv_flex_align_t (style_flex_main_place, style_flex_cross_place, style_flex_track_place)

- `start`
- `end`
- `center`
- `space_around`
- `space_between`
- `space_evenly`

### lv_grid_align_t (grid alignment)

- `start`
- `end`
- `center`
- `stretch`
- `space_around`
- `space_between`
- `space_evenly`

### lv_text_align_t (style_text_align)

- `left`
- `right`
- `center`
- `auto`

### lv_text_decor_t (style_text_decor)

- `none`
- `underline`
- `strikethrough`

### lv_dir_t (direction)

- `none`
- `top`
- `bottom`
- `left`
- `right`
- `hor`
- `ver`
- `all`

### lv_border_side_t (style_border_side)

- `none`
- `top`
- `bottom`
- `left`
- `right`
- `full`

### lv_grad_dir_t (style_bg_grad_dir)

- `none`
- `hor`
- `ver`

### lv_base_dir_t (style_base_dir)

- `auto`
- `ltr`
- `rtl`

### lv_layout_t (style_layout)

- `none`
- `flex`
- `grid`

### lv_blend_mode_t (style_blend_mode)

- `normal`
- `additive`
- `subtractive`
- `multiply`
- `difference`

---

## Critical Gotchas

### 1. The `zoom` Attribute Does NOT Exist

**❌ WRONG:**
```xml
<lv_image src="icon" zoom="128"/>
```

**✅ CORRECT:**
```xml
<lv_image src="icon" scale_x="128" scale_y="128"/>
```

**Source:** `lv_xml_image_parser.c:63-72`

The XML parser for `lv_image` only recognizes `scale_x` and `scale_y` (where 256 = 100%). There is no `zoom` attribute.

### 2. The `flex_align` Attribute Does NOT Work

**❌ WRONG:**
```xml
<lv_obj flex_flow="row" flex_align="center center center">
```

**✅ CORRECT:**
```xml
<lv_obj flex_flow="row"
        style_flex_main_place="center"
        style_flex_cross_place="center">
```

The `flex_align` attribute is silently ignored. Use `style_flex_main_place` and `style_flex_cross_place` instead.

### 3. Must Use Full Words, Not Abbreviations

**❌ WRONG:**
```xml
<lv_image src="icon" style_img_recolor="#ff0000" style_img_recolor_opa="255"/>
```

**✅ CORRECT:**
```xml
<lv_image src="icon" style_image_recolor="#ff0000" style_image_recolor_opa="255"/>
```

**Source:** `lv_xml_style.c:240-241`

The parser expects full words: `image` not `img`, `text` not `txt`.

### 4. Silent Failures

Unknown attributes are **silently ignored** without any warnings or errors. If an attribute doesn't work:

1. Check the parser source code in `lvgl/src/others/xml/parsers/`
2. Verify the exact attribute name (no abbreviations)
3. Confirm the attribute exists for that widget type
4. Test with known-working examples first

### 5. SIZE_CONTENT: XML Syntax

**CRITICAL:** In XML, use `"content"`, NOT `"LV_SIZE_CONTENT"`.

**✅ CORRECT:**
```xml
<lv_label text="Dynamic text" width="content" height="content"/>
<lv_obj flex_flow="column" width="100%" height="content">
    <!-- Children -->
</lv_obj>
```

**❌ WRONG - Parses as 0:**
```xml
<lv_label text="Dynamic text" width="LV_SIZE_CONTENT" height="LV_SIZE_CONTENT"/>
```

**Why:** The XML parser only recognizes the string `"content"`. Using `"LV_SIZE_CONTENT"` directly fails to parse correctly and evaluates to 0.

**In C++ code:** Use the constant `LV_SIZE_CONTENT`:
```cpp
lv_obj_set_width(obj, LV_SIZE_CONTENT);  // ✅ Correct in C++
```

**For complex layouts:** Call `lv_obj_update_layout()` after XML creation to ensure SIZE_CONTENT calculates correctly. See **docs/LV_SIZE_CONTENT_GUIDE.md** for complete details.

---

## Source Code References

All information in this document was extracted from LVGL 9.3 source code:

### Base Object Parser
- **File:** `lvgl/src/others/xml/parsers/lv_xml_obj_parser.c`
- **Lines:** 54-218 (attributes), 263-396 (style attributes)
- **Attributes:** x, y, width, height, align, flex_flow, flex_grow, ext_click_area, all flags, all states, binding attributes
- **Style Attributes:** 100+ style properties

### Widget Parsers

| Widget | Parser File | Key Lines |
|--------|-------------|-----------|
| lv_label | `lv_xml_label_parser.c` | 59-85 |
| lv_image | `lv_xml_image_parser.c` | 63-73 |
| lv_button | `lv_xml_button_parser.c` | 49-53 |
| lv_slider | `lv_xml_slider_parser.c` | 54-95 |
| lv_bar | `lv_xml_bar_parser.c` | 54-84 |
| lv_arc | `lv_xml_arc_parser.c` | 53-96 |
| lv_textarea | `lv_xml_textarea_parser.c` | 53-65 |
| lv_checkbox | `lv_xml_checkbox_parser.c` | 52-57 |
| lv_dropdown | `lv_xml_dropdown_parser.c` | 54-71 |
| lv_roller | `lv_xml_roller_parser.c` | 53-101 |
| lv_chart | `lv_xml_chart_parser.c` | 57-77 |
| lv_scale | `lv_xml_scale_parser.c` | 58-75 |
| lv_table | `lv_xml_table_parser.c` | 55-67 |
| lv_buttonmatrix | `lv_xml_buttonmatrix_parser.c` | 53-128 |
| lv_spangroup | `lv_xml_spangroup_parser.c` | 56-63 |

### Enum Definitions
- **File:** `lvgl/src/others/xml/lv_xml_base_types.c`
- **Lines:** 42-219
- **Enums:** align, state, flex_flow, flex_align, grid_align, text_align, text_decor, dir, border_side, grad_dir, base_dir, layout, blend_mode

---

## Contributing

This document is based on LVGL 9.3 source code analysis. When LVGL updates:

1. Re-read parser files in `lvgl/src/others/xml/parsers/`
2. Check `lv_xml_base_types.c` for new enum values
3. Update attribute lists and examples
4. Add new widgets or attributes to appropriate sections

**Version tracking:** Include LVGL version and commit hash when updating this document.

---

**Document Version:** 1.0
**LVGL Version:** 9.3
**Last Verified:** 2025-01-14
