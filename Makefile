# HelixScreen UI Prototype Makefile
# LVGL 9 + SDL2 simulator

# Build verbosity control
# Use 'make V=1' to see full compiler commands
V ?= 0
ifeq ($(V),0)
    Q := @
    ECHO := @echo
else
    Q :=
    ECHO := @true
endif

# Color output (disable with NO_COLOR=1)
ifndef NO_COLOR
    BOLD := \033[1m
    RED := \033[31m
    GREEN := \033[32m
    YELLOW := \033[33m
    BLUE := \033[34m
    MAGENTA := \033[35m
    CYAN := \033[36m
    RESET := \033[0m
else
    BOLD :=
    RED :=
    GREEN :=
    YELLOW :=
    BLUE :=
    MAGENTA :=
    CYAN :=
    RESET :=
endif

# Compilers
CC := clang
CXX := clang++
CFLAGS := -std=c11 -Wall -Wextra -O2 -g
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

# LVGL
LVGL_DIR := lvgl
LVGL_INC := -I$(LVGL_DIR) -I$(LVGL_DIR)/src
LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name "*.c")
LVGL_OBJS := $(patsubst $(LVGL_DIR)/%.c,$(OBJ_DIR)/lvgl/%.o,$(LVGL_SRCS))

# ThorVG sources (.cpp files for SVG support)
THORVG_SRCS := $(shell find $(LVGL_DIR)/src/libs/thorvg -name "*.cpp" 2>/dev/null)
THORVG_OBJS := $(patsubst $(LVGL_DIR)/%.cpp,$(OBJ_DIR)/lvgl/%.o,$(THORVG_SRCS))

# LVGL Demos (separate target)
LVGL_DEMO_SRCS := $(shell find $(LVGL_DIR)/demos -name "*.c")
LVGL_DEMO_OBJS := $(patsubst $(LVGL_DIR)/%.c,$(OBJ_DIR)/lvgl/%.o,$(LVGL_DEMO_SRCS))

# Application C++ sources (exclude test binaries)
APP_SRCS := $(filter-out $(SRC_DIR)/test_dynamic_cards.cpp,$(wildcard $(SRC_DIR)/*.cpp))
APP_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(APP_SRCS))

# Fonts
FONT_SRCS := assets/fonts/fa_icons_64.c assets/fonts/fa_icons_48.c assets/fonts/fa_icons_32.c assets/fonts/fa_icons_24.c assets/fonts/fa_icons_16.c assets/fonts/arrows_64.c assets/fonts/arrows_48.c assets/fonts/arrows_32.c
FONT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(FONT_SRCS))

# Material Design Icons
MATERIAL_ICON_SRCS := $(wildcard assets/images/material/*.c)
MATERIAL_ICON_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(MATERIAL_ICON_SRCS))

# SDL2
SDL2_CFLAGS := $(shell sdl2-config --cflags)
SDL2_LIBS := $(shell sdl2-config --libs)

# libhv (WebSocket client for Moonraker) - symlinked from parent repo submodule
LIBHV_DIR := libhv
LIBHV_INC := -I$(LIBHV_DIR)/include -I$(LIBHV_DIR)/cpputil
LIBHV_LIB := $(LIBHV_DIR)/lib/libhv.a

# spdlog (logging library) - symlinked from parent repo submodule
SPDLOG_DIR := spdlog
SPDLOG_INC := -I$(SPDLOG_DIR)/include

# Include paths
INCLUDES := -I. -I$(INC_DIR) $(LVGL_INC) $(LIBHV_INC) $(SPDLOG_INC) $(SDL2_CFLAGS)

# Platform detection and configuration
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    NPROC := $(shell sysctl -n hw.ncpu 2>/dev/null || echo 4)
    LDFLAGS := $(SDL2_LIBS) $(LIBHV_LIB) -lm -lpthread -framework CoreFoundation -framework Security
    PLATFORM := macOS
else
    # Linux
    NPROC := $(shell nproc 2>/dev/null || echo 4)
    LDFLAGS := $(SDL2_LIBS) $(LIBHV_LIB) -lm -lpthread
    PLATFORM := Linux
endif

# Parallel build control
# Set JOBS=N to override, or use -j directly
# Default: use all cores
JOBS ?= $(NPROC)

# Don't auto-enable parallel - let user control it
# Use 'make -j' or 'make JOBS=N' for parallel builds
ifneq ($(JOBS),1)
    MAKEFLAGS += --output-sync=target
endif

# Binary
TARGET := $(BIN_DIR)/helix-ui-proto

# LVGL configuration
LV_CONF := -DLV_CONF_INCLUDE_SIMPLE

# Test configuration
TEST_DIR := tests
TEST_UNIT_DIR := $(TEST_DIR)/unit
TEST_MOCK_DIR := $(TEST_DIR)/mocks
TEST_BIN := $(BIN_DIR)/run_tests
TEST_INTEGRATION_BIN := $(BIN_DIR)/run_integration_tests

# Unit tests (use real LVGL) - exclude mock example
TEST_SRCS := $(filter-out $(TEST_UNIT_DIR)/test_mock_example.cpp,$(wildcard $(TEST_UNIT_DIR)/*.cpp))
TEST_OBJS := $(patsubst $(TEST_UNIT_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))

# Integration tests (use mocks instead of real LVGL)
TEST_INTEGRATION_SRCS := $(TEST_UNIT_DIR)/test_mock_example.cpp
TEST_INTEGRATION_OBJS := $(patsubst $(TEST_UNIT_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_INTEGRATION_SRCS))

TEST_MAIN_OBJ := $(OBJ_DIR)/tests/test_main.o
CATCH2_OBJ := $(OBJ_DIR)/tests/catch_amalgamated.o
UI_TEST_UTILS_OBJ := $(OBJ_DIR)/tests/ui_test_utils.o

# Mock objects for integration testing
MOCK_SRCS := $(wildcard $(TEST_MOCK_DIR)/*.cpp)
MOCK_OBJS := $(patsubst $(TEST_MOCK_DIR)/%.cpp,$(OBJ_DIR)/tests/mocks/%.o,$(MOCK_SRCS))

.PHONY: all clean run test test-integration test-cards test-print-select test-size-content demo compile_commands libhv-build apply-patches generate-fonts help check-deps icon

# Default target
.DEFAULT_GOAL := all

# Help target
help:
	@echo "$(BOLD)HelixScreen UI Prototype Build System$(RESET)"
	@echo ""
	@echo "$(CYAN)Build Targets:$(RESET)"
	@echo "  $(GREEN)all$(RESET)              - Build the main binary (default)"
	@echo "  $(GREEN)build$(RESET)            - Clean build with progress (parallel: -j$(NPROC))"
	@echo "  $(GREEN)clean$(RESET)            - Remove all build artifacts"
	@echo "  $(GREEN)demo$(RESET)             - Build LVGL demo widgets"
	@echo ""
	@echo "$(CYAN)Test Targets:$(RESET)"
	@echo "  $(GREEN)test$(RESET)             - Run unit tests"
	@echo "  $(GREEN)test-integration$(RESET) - Run integration tests (with mocks)"
	@echo "  $(GREEN)test-cards$(RESET)       - Test dynamic card instantiation"
	@echo "  $(GREEN)test-print-select$(RESET) - Test print select panel"
	@echo ""
	@echo "$(CYAN)Run Targets:$(RESET)"
	@echo "  $(GREEN)run$(RESET)              - Build and run the UI prototype"
	@echo ""
	@echo "$(CYAN)Development Targets:$(RESET)"
	@echo "  $(GREEN)compile_commands$(RESET) - Generate compile_commands.json for IDE/LSP"
	@echo "  $(GREEN)check-deps$(RESET)       - Verify all dependencies are installed"
	@echo "  $(GREEN)apply-patches$(RESET)    - Apply LVGL patches"
	@echo "  $(GREEN)generate-fonts$(RESET)   - Regenerate FontAwesome fonts from package.json"
	@echo "  $(GREEN)icon$(RESET)             - Generate macOS .icns icon from logo"
	@echo "  $(GREEN)material-icons-list$(RESET) - List all registered Material Design icons"
	@echo "  $(GREEN)material-icons-convert$(RESET) - Convert SVGs to LVGL C arrays (SVGS=...)"
	@echo "  $(GREEN)material-icons-add$(RESET) - Download and add Material icons (ICONS=...)"
	@echo ""
	@echo "$(CYAN)Build Options:$(RESET)"
	@echo "  $(YELLOW)V=1$(RESET)              - Verbose mode (show full compiler commands)"
	@echo "  $(YELLOW)JOBS=N$(RESET)           - Set parallel job count (default: $(NPROC))"
	@echo "  $(YELLOW)NO_COLOR=1$(RESET)       - Disable colored output"
	@echo ""
	@echo "$(CYAN)Examples:$(RESET)"
	@echo "  make -j$(NPROC)          # Parallel build with all cores"
	@echo "  make V=1           # Verbose build"
	@echo "  make clean all     # Clean rebuild"
	@echo "  make run           # Build and run"
	@echo ""
	@echo "$(CYAN)Platform:$(RESET) $(PLATFORM) ($(NPROC) cores available)"

# Dependency checker
check-deps:
	$(ECHO) "$(CYAN)Checking build dependencies...$(RESET)"
	@ERROR=0; \
	if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "$(RED)✗ $(CC) not found$(RESET)"; ERROR=1; \
	else \
		echo "$(GREEN)✓ $(CC) found:$(RESET) $$($(CC) --version | head -n1)"; \
	fi; \
	if ! command -v $(CXX) >/dev/null 2>&1; then \
		echo "$(RED)✗ $(CXX) not found$(RESET)"; ERROR=1; \
	else \
		echo "$(GREEN)✓ $(CXX) found:$(RESET) $$($(CXX) --version | head -n1)"; \
	fi; \
	if ! command -v sdl2-config >/dev/null 2>&1; then \
		echo "$(RED)✗ SDL2 not found$(RESET)"; ERROR=1; \
		echo "  Install: $(YELLOW)brew install sdl2$(RESET) (macOS) or $(YELLOW)apt install libsdl2-dev$(RESET) (Linux)"; \
	else \
		echo "$(GREEN)✓ SDL2 found:$(RESET) $$(sdl2-config --version)"; \
	fi; \
	if [ ! -f "$(LIBHV_LIB)" ]; then \
		echo "$(YELLOW)⚠ libhv not built$(RESET)"; \
		echo "  Run: $(YELLOW)make libhv-build$(RESET)"; \
	else \
		echo "$(GREEN)✓ libhv found:$(RESET) $(LIBHV_LIB)"; \
	fi; \
	if [ ! -d "$(SPDLOG_DIR)/include" ]; then \
		echo "$(RED)✗ spdlog not found$(RESET)"; ERROR=1; \
	else \
		echo "$(GREEN)✓ spdlog found:$(RESET) $(SPDLOG_DIR)"; \
	fi; \
	if [ ! -d "$(LVGL_DIR)/src" ]; then \
		echo "$(RED)✗ LVGL not found$(RESET)"; ERROR=1; \
		echo "  Run: $(YELLOW)git submodule update --init --recursive$(RESET)"; \
	else \
		echo "$(GREEN)✓ LVGL found:$(RESET) $(LVGL_DIR)"; \
	fi; \
	if ! command -v npm >/dev/null 2>&1; then \
		echo "$(YELLOW)⚠ npm not found$(RESET) (needed for font generation)"; \
		echo "  Install: $(YELLOW)brew install node$(RESET) (macOS) or $(YELLOW)apt install npm$(RESET) (Linux)"; \
	else \
		echo "$(GREEN)✓ npm found:$(RESET) $$(npm --version)"; \
	fi; \
	if [ $$ERROR -eq 1 ]; then \
		echo ""; \
		echo "$(RED)Dependency check failed!$(RESET)"; \
		exit 1; \
	else \
		echo ""; \
		echo "$(GREEN)All dependencies satisfied!$(RESET)"; \
	fi

# Apply LVGL patches if not already applied
apply-patches:
	$(ECHO) "$(CYAN)Checking LVGL patches...$(RESET)"
	$(Q)if git -C $(LVGL_DIR) diff --quiet src/drivers/sdl/lv_sdl_window.c 2>/dev/null; then \
		echo "$(YELLOW)→ Applying LVGL SDL window position patch...$(RESET)"; \
		if git -C $(LVGL_DIR) apply --check ../patches/lvgl_sdl_window_position.patch 2>/dev/null; then \
			git -C $(LVGL_DIR) apply ../patches/lvgl_sdl_window_position.patch && \
			echo "$(GREEN)✓ Patch applied successfully$(RESET)"; \
		else \
			echo "$(YELLOW)⚠ Cannot apply patch (already applied or conflicts)$(RESET)"; \
		fi \
	else \
		echo "$(GREEN)✓ LVGL SDL window position patch already applied$(RESET)"; \
	fi

# Generate fonts if package.json is newer than stamp file
# Use stamp file pattern to avoid regenerating multiple times in parallel
.fonts.stamp: package.json
	$(ECHO) "$(CYAN)Checking font generation...$(RESET)"
	$(Q)if ! command -v npm >/dev/null 2>&1; then \
		echo "$(YELLOW)⚠ npm not found - skipping font generation$(RESET)"; \
		touch $@; \
	else \
		echo "$(YELLOW)→ Regenerating FontAwesome fonts from package.json...$(RESET)"; \
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

all: check-deps apply-patches generate-fonts $(TARGET)
	$(ECHO) "$(GREEN)$(BOLD)✓ Build complete!$(RESET)"
	$(ECHO) "$(CYAN)Run with: $(YELLOW)./$(TARGET)$(RESET)"

# Link binary
$(TARGET): $(APP_OBJS) $(LVGL_OBJS) $(THORVG_OBJS) $(FONT_OBJS) $(MATERIAL_ICON_OBJS)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)$(BOLD)[LD]$(RESET) $@"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) || { \
		echo "$(RED)$(BOLD)✗ Linking failed!$(RESET)"; \
		echo "$(YELLOW)Command:$(RESET) $(CXX) $(CXXFLAGS) [objects] -o $@ $(LDFLAGS)"; \
		exit 1; \
	}

# Collect all header dependencies
HEADERS := $(shell find $(INC_DIR) -name "*.h" 2>/dev/null)

# Compile app C++ sources (depend on headers)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[CXX]$(RESET) $<"
ifeq ($(V),1)
	$(Q)echo "$(YELLOW)Command:$(RESET) $(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@"
endif
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@ || { \
		echo "$(RED)$(BOLD)✗ Compilation failed:$(RESET) $<"; \
		echo "$(YELLOW)Command:$(RESET) $(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@"; \
		exit 1; \
	}

# Compile LVGL sources
$(OBJ_DIR)/lvgl/%.o: $(LVGL_DIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(CYAN)[CC]$(RESET) $<"
ifeq ($(V),1)
	$(Q)echo "$(YELLOW)Command:$(RESET) $(CC) $(CFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@"
endif
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@ || { \
		echo "$(RED)$(BOLD)✗ Compilation failed:$(RESET) $<"; \
		exit 1; \
	}

# Compile LVGL C++ sources (ThorVG)
$(OBJ_DIR)/lvgl/%.o: $(LVGL_DIR)/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(CYAN)[CXX]$(RESET) $<"
ifeq ($(V),1)
	$(Q)echo "$(YELLOW)Command:$(RESET) $(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@"
endif
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@ || { \
		echo "$(RED)$(BOLD)✗ Compilation failed:$(RESET) $<"; \
		exit 1; \
	}

# Compile font sources
$(OBJ_DIR)/assets/fonts/%.o: assets/fonts/%.c
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(GREEN)[FONT]$(RESET) $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@ || { \
		echo "$(RED)✗ Font compilation failed:$(RESET) $<"; \
		exit 1; \
	}

# Compile Material Design icon sources
$(OBJ_DIR)/assets/images/material/%.o: assets/images/material/%.c
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(GREEN)[ICON]$(RESET) $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@ || { \
		echo "$(RED)✗ Icon compilation failed:$(RESET) $<"; \
		exit 1; \
	}

# Run the prototype
run: $(TARGET)
	$(ECHO) "$(CYAN)Running UI prototype...$(RESET)"
	$(Q)$(TARGET)

# Clean build artifacts
clean-tests:
	$(ECHO) "$(YELLOW)Cleaning test artifacts...$(RESET)"
	$(Q)rm -f $(TEST_BIN) $(TEST_MAIN_OBJ) $(CATCH2_OBJ) $(UI_TEST_UTILS_OBJ) $(TEST_OBJS)
	$(ECHO) "$(GREEN)✓ Test artifacts cleaned$(RESET)"

clean:
	$(ECHO) "$(YELLOW)Cleaning build artifacts...$(RESET)"
	$(Q)if [ -d "$(BUILD_DIR)" ]; then \
		echo "$(YELLOW)→ Removing:$(RESET) $(BUILD_DIR)"; \
		rm -rf $(BUILD_DIR); \
		echo "$(GREEN)✓ Clean complete$(RESET)"; \
	else \
		echo "$(GREEN)✓ Already clean (no build directory)$(RESET)"; \
	fi
	$(Q)rm -f .fonts.stamp

# Parallel build target with progress
build:
	$(ECHO) "$(CYAN)$(BOLD)Starting clean parallel build...$(RESET)"
	$(ECHO) "$(CYAN)Platform:$(RESET) $(PLATFORM) | $(CYAN)Jobs:$(RESET) $(NPROC) | $(CYAN)Compiler:$(RESET) $(CXX)"
	@$(MAKE) clean
	@echo ""
	@echo "$(CYAN)Building with $(NPROC) parallel jobs...$(RESET)"
	@START_TIME=$$(date +%s); \
	$(MAKE) -j$(NPROC) all && \
	END_TIME=$$(date +%s); \
	DURATION=$$((END_TIME - START_TIME)); \
	echo ""; \
	echo "$(GREEN)$(BOLD)✓ Build completed in $${DURATION}s$(RESET)"


# Demo widgets target
demo: $(LVGL_OBJS) $(LVGL_DEMO_OBJS)
	$(ECHO) "$(MAGENTA)[LD]$(RESET) lvgl-demo"
	$(Q)$(CXX) -o $(BIN_DIR)/lvgl-demo demo-widgets.cpp $(LVGL_OBJS) $(LVGL_DEMO_OBJS) \
	  $(LVGL_INC) $(SDL2_CFLAGS) $(CXXFLAGS) $(SDL2_LIBS)
	$(ECHO) "$(GREEN)✓ Demo ready:$(RESET) ./build/bin/lvgl-demo"
	$(ECHO) ""
	$(ECHO) "$(CYAN)Run with:$(RESET) $(YELLOW)./build/bin/lvgl-demo$(RESET)"

$(OBJ_DIR)/lvgl/demos/%.o: $(LVGL_DIR)/demos/%.c
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(CYAN)[DEMO]$(RESET) $<"
	$(Q)$(CC) -c $< -o $@ $(LVGL_INC) $(CFLAGS)

# Test targets (Catch2 v3)
# Build tests in parallel
test-build:
	$(ECHO) "$(CYAN)$(BOLD)Building tests in parallel ($(NPROC) jobs)...$(RESET)"
	@START_TIME=$$(date +%s); \
	$(MAKE) -j$(NPROC) $(TEST_BIN) && \
	END_TIME=$$(date +%s); \
	DURATION=$$((END_TIME - START_TIME)); \
	echo "$(GREEN)✓ Tests built in $${DURATION}s$(RESET)"

# Unified test binary with all unit tests
test: $(TEST_BIN)
	$(ECHO) "$(CYAN)$(BOLD)Running unit tests...$(RESET)"
	$(Q)$(TEST_BIN) || { \
		echo "$(RED)$(BOLD)✗ Tests failed!$(RESET)"; \
		exit 1; \
	}
	$(ECHO) "$(GREEN)$(BOLD)✓ All tests passed!$(RESET)"

# Backwards compatibility alias
test-wizard: test

# Run only config tests
test-config: $(TEST_BIN)
	$(ECHO) "$(CYAN)$(BOLD)Running config tests...$(RESET)"
	$(Q)$(TEST_BIN) "[config]" || { \
		echo "$(RED)$(BOLD)✗ Config tests failed!$(RESET)"; \
		exit 1; \
	}
	$(ECHO) "$(GREEN)$(BOLD)✓ Config tests passed!$(RESET)"

$(TEST_BIN): $(TEST_MAIN_OBJ) $(CATCH2_OBJ) $(UI_TEST_UTILS_OBJ) $(TEST_OBJS) $(OBJ_DIR)/wizard_validation.o $(OBJ_DIR)/config.o $(LVGL_OBJS) $(THORVG_OBJS) $(OBJ_DIR)/ui_nav.o $(OBJ_DIR)/ui_temp_graph.o $(OBJ_DIR)/wifi_manager.o $(OBJ_DIR)/tips_manager.o $(OBJ_DIR)/ui_wizard.o $(OBJ_DIR)/ui_keyboard.o $(OBJ_DIR)/ui_switch.o $(OBJ_DIR)/moonraker_client.o $(OBJ_DIR)/printer_state.o $(LIBHV_LIB)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)$(BOLD)[LD]$(RESET) run_tests"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) || { \
		echo "$(RED)$(BOLD)✗ Test linking failed!$(RESET)"; \
		exit 1; \
	}
	$(ECHO) "$(GREEN)✓ Unit test binary ready$(RESET)"

# Integration test binary (uses mocks instead of real LVGL)
$(TEST_INTEGRATION_BIN): $(TEST_MAIN_OBJ) $(CATCH2_OBJ) $(TEST_INTEGRATION_OBJS) $(MOCK_OBJS)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)$(BOLD)[LD]$(RESET) run_integration_tests"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) || { \
		echo "$(RED)$(BOLD)✗ Integration test linking failed!$(RESET)"; \
		exit 1; \
	}
	$(ECHO) "$(GREEN)✓ Integration test binary ready$(RESET)"

# Run integration tests
test-integration: $(TEST_INTEGRATION_BIN)
	$(ECHO) "$(CYAN)$(BOLD)Running integration tests (with mocks)...$(RESET)"
	$(Q)$(TEST_INTEGRATION_BIN) || { \
		echo "$(RED)$(BOLD)✗ Integration tests failed!$(RESET)"; \
		exit 1; \
	}
	$(ECHO) "$(GREEN)$(BOLD)✓ All integration tests passed!$(RESET)"

# Compile test main (Catch2 runner)
$(TEST_MAIN_OBJ): $(TEST_DIR)/test_main.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST-MAIN]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_DIR) -c $< -o $@

# Compile Catch2 amalgamated source
$(CATCH2_OBJ): $(TEST_DIR)/catch_amalgamated.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[CATCH2]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile UI test utilities
$(UI_TEST_UTILS_OBJ): $(TEST_DIR)/ui_test_utils.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(CYAN)[UI-TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_DIR) $(INCLUDES) -c $< -o $@

# Compile test sources
$(OBJ_DIR)/tests/%.o: $(TEST_UNIT_DIR)/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_DIR) $(INCLUDES) -c $< -o $@

# Compile mock sources
$(OBJ_DIR)/tests/mocks/%.o: $(TEST_MOCK_DIR)/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(YELLOW)[MOCK]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_MOCK_DIR) $(INCLUDES) -c $< -o $@

# Dynamic card instantiation test
TEST_CARDS_BIN := $(BIN_DIR)/test_dynamic_cards
TEST_CARDS_OBJ := $(OBJ_DIR)/test_dynamic_cards.o

test-cards: $(TEST_CARDS_BIN)
	$(ECHO) "$(CYAN)Running dynamic card test...$(RESET)"
	$(Q)$(TEST_CARDS_BIN)

$(TEST_CARDS_BIN): $(TEST_CARDS_OBJ) $(LVGL_OBJS) $(FONT_OBJS)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)[LD]$(RESET) test_dynamic_cards"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	$(ECHO) "$(GREEN)✓ Test binary ready$(RESET)"

$(TEST_CARDS_OBJ): $(SRC_DIR)/test_dynamic_cards.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@

# Print select panel test with mock data
TEST_PRINT_SELECT_BIN := $(BIN_DIR)/test_print_select_panel
TEST_PRINT_SELECT_OBJ := $(OBJ_DIR)/tests/test_print_select_panel.o
MOCK_FILES_OBJ := $(OBJ_DIR)/tests/mock_print_files.o

test-print-select: $(TEST_PRINT_SELECT_BIN)
	$(ECHO) "$(CYAN)Running print select panel test...$(RESET)"
	$(Q)$(TEST_PRINT_SELECT_BIN)

$(TEST_PRINT_SELECT_BIN): $(TEST_PRINT_SELECT_OBJ) $(MOCK_FILES_OBJ) $(LVGL_OBJS)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)[LD]$(RESET) test_print_select_panel"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	$(ECHO) "$(GREEN)✓ Test binary ready$(RESET)"

$(TEST_PRINT_SELECT_OBJ): $(TEST_DIR)/test_print_select_panel.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_DIR) $(INCLUDES) $(LV_CONF) -c $< -o $@

$(MOCK_FILES_OBJ): $(TEST_DIR)/mock_print_files.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) -I$(TEST_DIR) $(INCLUDES) $(LV_CONF) -c $< -o $@

# LV_SIZE_CONTENT behavior test
TEST_SIZE_CONTENT_BIN := $(BIN_DIR)/test_size_content
TEST_SIZE_CONTENT_OBJ := $(OBJ_DIR)/test_size_content.o

test-size-content: $(TEST_SIZE_CONTENT_BIN)
	$(ECHO) "$(CYAN)Running LV_SIZE_CONTENT behavior test...$(RESET)"
	$(Q)$(TEST_SIZE_CONTENT_BIN)

$(TEST_SIZE_CONTENT_BIN): $(TEST_SIZE_CONTENT_OBJ) $(LVGL_OBJS) $(THORVG_OBJS)
	$(Q)mkdir -p $(BIN_DIR)
	$(ECHO) "$(MAGENTA)[LD]$(RESET) test_size_content"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	$(ECHO) "$(GREEN)✓ Test binary ready: $@$(RESET)"

$(TEST_SIZE_CONTENT_OBJ): test_size_content.cpp
	$(Q)mkdir -p $(dir $@)
	$(ECHO) "$(BLUE)[TEST]$(RESET) $<"
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDES) $(LV_CONF) -c $< -o $@

# Generate compile_commands.json for IDE/LSP support
compile_commands:
	$(ECHO) "$(CYAN)Checking for bear...$(RESET)"
	$(Q)if ! command -v bear >/dev/null 2>&1; then \
		echo "$(RED)Error: 'bear' not found$(RESET)"; \
		echo "Install with: $(YELLOW)brew install bear$(RESET) (macOS) or $(YELLOW)apt install bear$(RESET) (Linux)"; \
		exit 1; \
	fi
	$(ECHO) "$(CYAN)Generating compile_commands.json...$(RESET)"
	$(Q)bear -- $(MAKE) clean
	$(Q)bear -- $(MAKE) -j$(NPROC)
	$(ECHO) "$(GREEN)✓ compile_commands.json generated$(RESET)"
	$(ECHO) ""
	$(ECHO) "$(CYAN)IDE/LSP integration ready. Restart your editor to pick up changes.$(RESET)"

# Build libhv (configure + compile)
# NOTE: This will be used when libhv is not a symlink but a real submodule
libhv-build:
	$(ECHO) "$(CYAN)Building libhv...$(RESET)"
	$(Q)cd $(LIBHV_DIR) && ./configure --with-http-client
	$(Q)$(MAKE) -C $(LIBHV_DIR) -j$(NPROC) libhv
	$(ECHO) "$(GREEN)✓ libhv built successfully$(RESET)"

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
		-crop 1024x640+0+40 +repage \
		-gravity center -background none -extent 680x680 \
		assets/images/helix-icon.png
	$(ECHO) "$(CYAN)  [2/6] Generating 64x64 icon for window...$(RESET)"
	$(Q)magick assets/images/helix-icon.png -resize 64x64 assets/images/helix-icon-64.png
	$(ECHO) "$(CYAN)  [3/6] Generating C header file for embedded icon...$(RESET)"
	$(Q)python3 scripts/generate_icon_header.py assets/images/helix-icon-64.png include/helix_icon_data.h
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
	$(ECHO) "$(GREEN)✓ Icon generated: assets/images/helix-icon.icns + helix-icon-64.png + header$(RESET)"
	@ls -lh assets/images/helix-icon.icns assets/images/helix-icon-64.png include/helix_icon_data.h | awk '{print "$(CYAN)  " $$9 ": " $$5 "$(RESET)"}'
else
	$(ECHO) "$(CYAN)  [4/4] Icon generated (PNG format)...$(RESET)"
	$(ECHO) "$(GREEN)✓ Icon generated: assets/images/helix-icon.png + helix-icon-64.png + header$(RESET)"
	@ls -lh assets/images/helix-icon.png assets/images/helix-icon-64.png include/helix_icon_data.h | awk '{print "$(CYAN)  " $$9 ": " $$5 "$(RESET)"}'
	$(ECHO) "$(YELLOW)Note: .icns format requires macOS. PNG icons can be used for Linux apps.$(RESET)"
endif

