# bandwidth3 – Build & install script
# --------------------------------------------------------------
#  Targets:
#     make / make release  – optimised binary in ./build/
#     make debug           – symbols / no optimisation
#     make test            – unit + smoke tests (assert-only)
#     make docs            – generate man page (help2man) if available
#     make install         – copy binary & man page (root → /usr/local, else → ~/.local)
#     make uninstall       – remove installed artifacts
# --------------------------------------------------------------

# -------- Project metadata ---------------------------
TARGET   ?= bandwidth3
VERSION  ?= 0.1.6
PREFIX   ?= /usr/local        # honourable default; may be overridden

BUILD_DIR := build
SRC       := $(wildcard src/*.c)
OBJ       := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))

CC      ?= gcc
CFLAGS_common  = -std=c11 -Wall -Wextra -pedantic -DBANDWIDTH3_VERSION=\"$(VERSION)\"
CFLAGS_rel     = -O2 -DNDEBUG
CFLAGS_dbg     = -g -O0
LDFLAGS        =
INCLUDES			 = -I$(BUILD_DIR) -I./include/

# -------- Commit Hash for release archive ------------
COMMIT_HASH_STUB := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

# -------- Default rule (release) ----------------------
all release: CFLAGS += $(CFLAGS_common) $(CFLAGS_rel)
all release: $(BUILD_DIR)/$(TARGET)

debug:   CFLAGS += $(CFLAGS_common) $(CFLAGS_dbg)
debug:   $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	@rm -rf $(BUILD_DIR)

# -------- Tests --------------------------------------
TEST_SRC := tests/test_basic.c src/net_stats.c
test: $(BUILD_DIR)/test_basic $(BUILD_DIR)/$(TARGET) tests/smoke.sh
	@echo "Running unit tests …" && $(BUILD_DIR)/test_basic
	@echo "Running smoke test …" && sh tests/smoke.sh $(BUILD_DIR)/$(TARGET)
	@echo "All tests passed ✔"

$(BUILD_DIR)/test_basic: $(TEST_SRC) | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(CFLAGS_common) -g -O0 $^ -o $@

# -------- Documentation (man page) -------------------
# docs: docs/$(TARGET).1

# help2man produces nice roff; fallback to existing stub if tool absent
DOC_TMP := $(BUILD_DIR)/$(TARGET)_help.txt
docs/$(TARGET).1: $(BUILD_DIR)/$(TARGET)
	@mkdir -p $(@D) # Ensure the 'docs' directory exists
	@if command -v help2man >/dev/null 2>&1 ; then \
		help2man -N \
		--no-info --version-string=$(VERSION) \
		--include docs/help-includes \
		--name "Polybar bandwidth module" $< -o $@ ; \
	else \
		echo "help2man not found – using stub" ; \
		[ -f $@ ] || cp docs/stub.1 $@ ; \
	fi
# -------- Install / uninstall ------------------------
#
# Rules:
#   • If the caller leaves PREFIX at its default (/usr/local)
#     *and* is *not* root → install privately to $HOME/.local
#   • Otherwise honour the given PREFIX verbatim.
#
# ifeq ($(PREFIX),/usr/local)

ifneq ($(shell id -u),0)           # UID ≠ 0  → not root
    INSTALL_PREFIX := $(HOME)/.local
else                               # running as root
    INSTALL_PREFIX := $(PREFIX)
endif

# else                                  # custom PREFIX supplied
# INSTALL_PREFIX := $(PREFIX)
# endif

BINDIR  := $(INSTALL_PREFIX)/bin
MANDIR  := $(INSTALL_PREFIX)/share/man/man1

install: $(BUILD_DIR)/$(TARGET) docs/$(TARGET).1
	@echo "Installing to $(INSTALL_PREFIX)"
	@install -d "$(BINDIR)" "$(MANDIR)"
	@install -m 755 "$(BUILD_DIR)/$(TARGET)" "$(BINDIR)/$(TARGET)"
	@install -m 644 "docs/$(TARGET).1" "$(MANDIR)/$(TARGET).1"
	@echo "Done."

uninstall:
	@echo "Removing $(BINDIR)/$(TARGET)"
	@rm -f "$(BINDIR)/$(TARGET)"
	@rm -f "$(MANDIR)/$(TARGET).1"


# -------- Release archive ----------------------------
ARCHIVE_NAME := bandwidth3-$(VERSION)-$(COMMIT_HASH_STUB).tar.gz
dist: $(BUILD_DIR)/$(TARGET) docs/$(TARGET).1
	@echo "Creating release archive: $(ARCHIVE_NAME)"
	@mkdir -p dist
	tar -czvf dist/$(ARCHIVE_NAME) \
		$(BUILD_DIR)/$(TARGET) \
		README.md \
		docs/$(TARGET).1 \
		Makefile
	@echo "Archive created: dist/$(ARCHIVE_NAME)"

.PHONY: all release debug clean test docs install uninstall dist
