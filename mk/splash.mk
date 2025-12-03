# SPDX-License-Identifier: GPL-3.0-or-later
# Splash screen binary build rules
#
# Builds helix-splash, a minimal splash screen for embedded targets.
# This binary starts instantly while the main app initializes in parallel.
#
# Only built for embedded Linux targets (not macOS/desktop).

SPLASH_SRC := src/helix_splash.cpp
SPLASH_OBJ := $(BUILD_DIR)/splash/helix_splash.o
SPLASH_BIN := $(BUILD_DIR)/bin/helix-splash

# Splash needs LVGL and display library, nothing else
SPLASH_CXXFLAGS := $(CXXFLAGS) $(LVGL_INC) -DHELIX_SPLASH_ONLY
SPLASH_LDFLAGS := $(DISPLAY_LIB) $(LVGL_LIB) -lm -lpthread

# Add platform-specific libraries for display backend
ifneq ($(UNAME_S),Darwin)
    # Linux: may need DRM/input libraries
    ifeq ($(DISPLAY_BACKEND),drm)
        SPLASH_LDFLAGS += -ldrm -linput
    endif
endif

# Only build splash for embedded targets (not macOS)
# On macOS, the main app handles its own splash via SDL
ifneq ($(UNAME_S),Darwin)

# Compile splash source
$(BUILD_DIR)/splash/%.o: src/%.cpp | $(BUILD_DIR)/splash
	@echo "[CXX] $< (splash)"
	$(Q)$(CXX) $(SPLASH_CXXFLAGS) -c $< -o $@

# Link splash binary
$(SPLASH_BIN): $(SPLASH_OBJ) $(DISPLAY_LIB) $(LVGL_LIB) | $(BUILD_DIR)/bin
	@echo "[LD] $@"
	$(Q)$(CXX) $(SPLASH_OBJ) -o $@ $(SPLASH_LDFLAGS)

# Create build directory
$(BUILD_DIR)/splash:
	$(Q)mkdir -p $@

# Build splash binary
.PHONY: splash
splash: $(SPLASH_BIN)
	@echo "Built: $(SPLASH_BIN)"

# Clean splash build artifacts
.PHONY: clean-splash
clean-splash:
	$(Q)rm -rf $(BUILD_DIR)/splash $(SPLASH_BIN)

else
# macOS: no-op targets

.PHONY: splash clean-splash
splash:
	@echo "Splash binary not built on macOS (uses SDL splash in main app)"

clean-splash:
	@true

endif  # not Darwin

# Help text
.PHONY: help-splash
help-splash:
	@echo "Splash screen targets:"
	@echo "  splash        - Build helix-splash binary (embedded only)"
	@echo "  clean-splash  - Remove splash build artifacts"
