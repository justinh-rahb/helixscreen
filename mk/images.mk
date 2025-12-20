# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixScreen - Pre-rendered Image Generation Module
#
# Pre-renders PNG images to LVGL binary format for each supported screen size.
# This eliminates runtime PNG decoding, dramatically improving startup performance
# on embedded devices (2 FPS → 116 FPS on AD5M).
#
# Images are generated to the BUILD directory (not committed to repo).
# They are created automatically during embedded builds and deployments.
#
# Splash screen targets:
#   gen-images-ad5m  - Generate only 800x480 (AD5M fixed display)
#   gen-images-pi    - Generate all sizes (Pi variable displays)
#   gen-images       - Generate all sizes (generic)
#
# Printer image targets:
#   gen-printer-images - Pre-render all printer images at 300px and 150px
#
# Output:
#   build/assets/images/prerendered/*.bin        (splash)
#   build/assets/images/printers/prerendered/*.bin (printers)
#
# See: docs/PRE_RENDERED_IMAGES.md

PRERENDERED_DIR := $(BUILD_DIR)/assets/images/prerendered
PRERENDERED_PRINTERS_DIR := $(BUILD_DIR)/assets/images/printers/prerendered
REGEN_IMAGES_SCRIPT := scripts/regen_images.sh
REGEN_PRINTER_IMAGES_SCRIPT := scripts/regen_printer_images.sh

# Pre-rendered image files (build artifacts, not in repo)
# AD5M only needs 'small' (800x480)
PRERENDERED_IMAGES_AD5M := $(PRERENDERED_DIR)/splash-logo-small.bin

# Pi needs all sizes (unknown display at build time)
PRERENDERED_IMAGES_ALL := \
    $(PRERENDERED_DIR)/splash-logo-tiny.bin \
    $(PRERENDERED_DIR)/splash-logo-small.bin \
    $(PRERENDERED_DIR)/splash-logo-medium.bin \
    $(PRERENDERED_DIR)/splash-logo-large.bin

# Generate images for AD5M (800x480 fixed display only)
# NOTE: Uses mkdir -p instead of $(BUILD_DIR) dependency to avoid triggering 'build' target
# which runs 'make clean' and wipes cross-compiled binaries
.PHONY: gen-images-ad5m
gen-images-ad5m:
	$(ECHO) "$(CYAN)Generating pre-rendered images for AD5M (800x480)...$(RESET)"
	$(Q)mkdir -p $(PRERENDERED_DIR)
	$(Q)OUTPUT_DIR=$(PRERENDERED_DIR) TARGET_SIZES=small ./$(REGEN_IMAGES_SCRIPT)
	$(ECHO) "$(GREEN)✓ AD5M images generated$(RESET)"

# Generate images for Pi (all sizes for variable displays)
.PHONY: gen-images-pi
gen-images-pi:
	$(ECHO) "$(CYAN)Generating pre-rendered images for Pi (all sizes)...$(RESET)"
	$(Q)mkdir -p $(PRERENDERED_DIR)
	$(Q)OUTPUT_DIR=$(PRERENDERED_DIR) ./$(REGEN_IMAGES_SCRIPT)
	$(ECHO) "$(GREEN)✓ Pi images generated$(RESET)"

# Generate all pre-rendered images (generic)
.PHONY: gen-images
gen-images:
	$(ECHO) "$(CYAN)Generating pre-rendered images (all sizes)...$(RESET)"
	$(Q)mkdir -p $(PRERENDERED_DIR)
	$(Q)OUTPUT_DIR=$(PRERENDERED_DIR) ./$(REGEN_IMAGES_SCRIPT)
	$(ECHO) "$(GREEN)✓ Pre-rendered images generated in $(PRERENDERED_DIR)/$(RESET)"

# Legacy alias
.PHONY: regen-images
regen-images: gen-images

# Clean generated images
.PHONY: clean-images
clean-images:
	$(ECHO) "$(CYAN)Cleaning pre-rendered images...$(RESET)"
	$(Q)rm -rf $(PRERENDERED_DIR)
	$(ECHO) "$(GREEN)✓ Cleaned $(PRERENDERED_DIR)$(RESET)"

# List what would be generated
.PHONY: list-images
list-images:
	$(Q)./$(REGEN_IMAGES_SCRIPT) --list

# Check if pre-rendered images exist in build directory
.PHONY: check-images
check-images:
	$(ECHO) "$(CYAN)Checking pre-rendered images...$(RESET)"
	$(Q)missing=0; \
	for img in $(PRERENDERED_IMAGES_ALL); do \
		if [ ! -f "$$img" ]; then \
			echo "$(RED)✗ Missing: $$img$(RESET)"; \
			missing=1; \
		fi; \
	done; \
	if [ $$missing -eq 1 ]; then \
		echo "$(RED)Run 'make gen-images' to generate missing files$(RESET)"; \
		exit 1; \
	else \
		echo "$(GREEN)✓ All pre-rendered images present$(RESET)"; \
	fi

# =============================================================================
# Printer Image Pre-rendering
# =============================================================================
# Generates optimized versions of printer images at 300px and 150px
# Original PNGs kept as fallbacks

# Generate pre-rendered printer images (all sizes)
# NOTE: Uses mkdir -p instead of $(BUILD_DIR) dependency (see gen-images-ad5m comment)
.PHONY: gen-printer-images
gen-printer-images:
	$(ECHO) "$(CYAN)Generating pre-rendered printer images...$(RESET)"
	$(Q)mkdir -p $(PRERENDERED_PRINTERS_DIR)
	$(Q)OUTPUT_DIR=$(PRERENDERED_PRINTERS_DIR) ./$(REGEN_PRINTER_IMAGES_SCRIPT)
	$(ECHO) "$(GREEN)✓ Printer images generated$(RESET)"

# Clean pre-rendered printer images
.PHONY: clean-printer-images
clean-printer-images:
	$(ECHO) "$(CYAN)Cleaning pre-rendered printer images...$(RESET)"
	$(Q)rm -rf $(PRERENDERED_PRINTERS_DIR)
	$(ECHO) "$(GREEN)✓ Cleaned $(PRERENDERED_PRINTERS_DIR)$(RESET)"

# List printer images that would be generated
.PHONY: list-printer-images
list-printer-images:
	$(Q)./$(REGEN_PRINTER_IMAGES_SCRIPT) --list

# Generate ALL pre-rendered images (splash + printers)
.PHONY: gen-all-images
gen-all-images: gen-images gen-printer-images

# Clean ALL pre-rendered images
.PHONY: clean-all-images
clean-all-images: clean-images clean-printer-images

# =============================================================================
# Help
# =============================================================================

# Help text for image targets
.PHONY: help-images
help-images:
	@echo "Pre-rendered image targets:"
	@echo ""
	@echo "  Splash screen:"
	@echo "    gen-images         - Generate splash .bin files (all sizes)"
	@echo "    gen-images-ad5m    - Generate splash for AD5M only (800x480)"
	@echo "    gen-images-pi      - Generate splash for Pi (all sizes)"
	@echo "    clean-images       - Remove splash .bin files"
	@echo "    list-images        - Show splash targets"
	@echo ""
	@echo "  Printer images:"
	@echo "    gen-printer-images - Generate printer .bin files (300px, 150px)"
	@echo "    clean-printer-images - Remove printer .bin files"
	@echo "    list-printer-images  - Show printer targets"
	@echo ""
	@echo "  Combined:"
	@echo "    gen-all-images     - Generate all pre-rendered images"
	@echo "    clean-all-images   - Remove all pre-rendered images"
	@echo ""
	@echo "Output directories:"
	@echo "  $(PRERENDERED_DIR)/"
	@echo "  $(PRERENDERED_PRINTERS_DIR)/"
	@echo ""
	@echo "Note: Generated images are build artifacts, not committed to repo."
	@echo "      They are created during deploy-* and release builds."
	@echo ""
	@echo "Performance impact:"
	@echo "  PNG decoding:    ~2 FPS during splash (AD5M)"
	@echo "  Pre-rendered:    ~116 FPS (instant display)"
