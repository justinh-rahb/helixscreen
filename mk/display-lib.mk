# SPDX-License-Identifier: GPL-3.0-or-later
# Static library for display backend - shared by splash and main app
#
# This library contains the display backend abstraction layer:
# - DisplayBackend base class and factory
# - Platform-specific implementations (SDL, fbdev, DRM)
#
# Both helix-splash and helix-ui-proto link against this library,
# ensuring consistent display detection and initialization.

DISPLAY_LIB := $(BUILD_DIR)/lib/libhelix-display.a

# Core display backend sources (always included)
DISPLAY_SRCS := \
    src/display_backend.cpp

# Platform-specific backends
ifeq ($(UNAME_S),Darwin)
    # macOS: SDL only
    DISPLAY_SRCS += src/display_backend_sdl.cpp
else
    # Linux: framebuffer and DRM for embedded, SDL for desktop
    DISPLAY_SRCS += src/display_backend_fbdev.cpp
    DISPLAY_SRCS += src/display_backend_drm.cpp
    ifndef CROSS_COMPILE
        # Native Linux desktop also gets SDL
        DISPLAY_SRCS += src/display_backend_sdl.cpp
    endif
endif

DISPLAY_OBJS := $(DISPLAY_SRCS:src/%.cpp=$(BUILD_DIR)/display/%.o)

# Display library needs LVGL headers
DISPLAY_CXXFLAGS := $(CXXFLAGS) $(LVGL_INC)

# Build object files
$(BUILD_DIR)/display/%.o: src/%.cpp | $(BUILD_DIR)/display
	@echo "[CXX] $<"
	$(Q)$(CXX) $(DISPLAY_CXXFLAGS) -c $< -o $@

# Build static library
$(DISPLAY_LIB): $(DISPLAY_OBJS) | $(BUILD_DIR)/lib
	@echo "[AR] $@"
	$(Q)$(AR) rcs $@ $^

# Create directories
$(BUILD_DIR)/display:
	$(Q)mkdir -p $@

# Phony target for building just the display library
.PHONY: display-lib
display-lib: $(DISPLAY_LIB)

# Clean target
.PHONY: clean-display-lib
clean-display-lib:
	$(Q)rm -rf $(BUILD_DIR)/display $(DISPLAY_LIB)
