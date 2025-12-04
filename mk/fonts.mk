# Copyright (c) 2025 Preston Brown <pbrown@brown-house.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixScreen UI Prototype - Font & Icon Generation Module
# Handles font generation and Material Design icons

# Generate fonts if package.json is newer than stamp file
# Use stamp file pattern to avoid regenerating multiple times in parallel
.fonts.stamp: package.json
	$(ECHO) "$(CYAN)Checking font generation...$(RESET)"
	$(Q)if ! command -v npm >/dev/null 2>&1; then \
		echo "$(YELLOW)⚠ npm not found - skipping font generation$(RESET)"; \
		touch $@; \
	else \
		echo "$(YELLOW)→ Regenerating MDI icon fonts from package.json...$(RESET)"; \
		if [ "$(PLATFORM)" = "macOS" ]; then \
			npm run convert-all-fonts && touch $@ && echo "$(GREEN)✓ Fonts regenerated successfully$(RESET)"; \
		else \
			npm run convert-fonts-ci && touch $@ && echo "$(GREEN)✓ Fonts regenerated successfully (arrow fonts skipped on Linux)$(RESET)"; \
		fi \
	fi

# Fonts depend on stamp file to ensure they're regenerated when needed
$(FONT_SRCS): .fonts.stamp

generate-fonts: .fonts.stamp

# Material Design Icon Management
material-icons-list:
	@.venv/bin/python3 scripts/material_icons.py list

material-icons-convert:
ifndef SVGS
	@echo "$(RED)Error: SVGS not specified$(RESET)"
	@echo "Usage: make material-icons-convert SVGS=\"icon1.svg icon2.svg ...\""
	@exit 1
endif
	@.venv/bin/python3 scripts/material_icons.py convert $(SVGS)

material-icons-add:
ifndef ICONS
	@echo "$(RED)Error: ICONS not specified$(RESET)"
	@echo "Usage: make material-icons-add ICONS=\"wifi-strength-1 wifi-strength-2 ...\""
	@echo ""
	@echo "$(CYAN)This will:$(RESET)"
	@echo "  1. Download SVGs from Material Design Icons (google/material-design-icons)"
	@echo "  2. Convert to 64x64 PNGs"
	@echo "  3. Generate LVGL C arrays"
	@echo "  4. Register in material_icons.h/.cpp"
	@exit 1
endif
	@.venv/bin/python3 scripts/material_icons.py add $(ICONS)

# Generate macOS .icns icon from source logo
# Requires: ImageMagick (magick) for image processing
# Source: assets/images/helixscreen-logo.png
# Output: assets/images/helix-icon.icns (macOS), assets/images/helix-icon.png (Linux)
icon:
ifeq ($(UNAME_S),Darwin)
	$(ECHO) "$(CYAN)Generating macOS icon from logo...$(RESET)"
	@if ! command -v magick >/dev/null 2>&1; then \
		echo "$(RED)✗ ImageMagick (magick) not found$(RESET)"; \
		echo "$(YELLOW)Install with: brew install imagemagick$(RESET)"; \
		exit 1; \
	fi
	@if ! command -v iconutil >/dev/null 2>&1; then \
		echo "$(RED)✗ iconutil not found (should be built-in on macOS)$(RESET)"; \
		exit 1; \
	fi
else
	$(ECHO) "$(CYAN)Generating icon from logo (Linux - PNG only)...$(RESET)"
	@if ! command -v magick >/dev/null 2>&1; then \
		echo "$(RED)✗ ImageMagick (magick) not found$(RESET)"; \
		echo "$(YELLOW)Install with: sudo apt install imagemagick$(RESET)"; \
		exit 1; \
	fi
endif
	$(ECHO) "$(CYAN)  [1/6] Cropping logo to circular icon...$(RESET)"
	$(Q)magick assets/images/helixscreen-logo.png \
		-crop 700x580+162+100 +repage \
		-gravity center -background none -extent 680x680 \
		assets/images/helix-icon.png
	$(ECHO) "$(CYAN)  [2/6] Generating 128x128 icon for window...$(RESET)"
	$(Q)magick assets/images/helix-icon.png -resize 128x128 assets/images/helix-icon-128.png
	$(ECHO) "$(CYAN)  [3/6] Generating C header file for embedded icon...$(RESET)"
	$(Q)python3 scripts/generate_icon_header.py assets/images/helix-icon-128.png include/helix_icon_data.h
ifeq ($(UNAME_S),Darwin)
	$(ECHO) "$(CYAN)  [4/6] Generating icon sizes (16px to 1024px)...$(RESET)"
	$(Q)mkdir -p assets/images/icon.iconset
	$(Q)for size in 16 32 64 128 256 512; do \
		magick assets/images/helix-icon.png -resize $${size}x$${size} \
			assets/images/icon.iconset/icon_$${size}x$${size}.png; \
		magick assets/images/helix-icon.png -resize $$((size*2))x$$((size*2)) \
			assets/images/icon.iconset/icon_$${size}x$${size}@2x.png; \
	done
	$(ECHO) "$(CYAN)  [5/6] Creating .icns bundle...$(RESET)"
	$(Q)iconutil -c icns assets/images/icon.iconset -o assets/images/helix-icon.icns
	$(ECHO) "$(CYAN)  [6/6] Cleaning up temporary files...$(RESET)"
	$(Q)rm -rf assets/images/icon.iconset
	$(ECHO) "$(GREEN)✓ Icon generated: assets/images/helix-icon.icns + helix-icon-128.png + header$(RESET)"
	@ls -lh assets/images/helix-icon.icns assets/images/helix-icon-128.png include/helix_icon_data.h | awk '{print "$(CYAN)  " $$9 ": " $$5 "$(RESET)"}'
else
	$(ECHO) "$(CYAN)  [4/4] Icon generated (PNG format)...$(RESET)"
	$(ECHO) "$(GREEN)✓ Icon generated: assets/images/helix-icon.png + helix-icon-128.png + header$(RESET)"
	@ls -lh assets/images/helix-icon.png assets/images/helix-icon-128.png include/helix_icon_data.h | awk '{print "$(CYAN)  " $$9 ": " $$5 "$(RESET)"}'
	$(ECHO) "$(YELLOW)Note: .icns format requires macOS. PNG icons can be used for Linux apps.$(RESET)"
endif
