TARGET_EXEC := ntremu.wasm

CC := clang --target=wasm32 --sysroot=../../wasmlite/libc -nodefaultlibs

CFLAGS := -Wall -Wimplicit-fallthrough -Wno-format #-Werror
CFLAGS_RELEASE := -Oz -ffast-math -flto
CFLAGS_DEBUG := -g -DCPULOG

CPPFLAGS := -MP -MMD

LDFLAGS_RELEASE := -lm -lc
LDFLAGS_DEBUG := -lm -lc-dbg
LDFLAGS += -Wl,--export-table -Wl,--export=malloc

ifeq ($(shell uname),Darwin)
	CPPFLAGS += -I/opt/homebrew/include
	LDFLAGS := -L/opt/homebrew/lib $(LDFLAGS)
endif

BUILD_DIR := build
SRC_DIR := src

DEBUG_DIR := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

SRCS := $(shell find $(SRC_DIR) -name '*.c')
SRCS := $(SRCS:$(SRC_DIR)/%=%)

OBJS_DEBUG := $(SRCS:%.c=$(DEBUG_DIR)/%.o)
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)

OBJS_RELEASE := $(SRCS:%.c=$(RELEASE_DIR)/%.o)
DEPS_RELEASE := $(OBJS_RELEASE:.o=.d)

.PHONY: release, debug, clean

release: CFLAGS += $(CFLAGS_RELEASE)
release: LDFLAGS += $(LDFLAGS_RELEASE)
release: $(RELEASE_DIR)/$(TARGET_EXEC)

debug: CFLAGS += $(CFLAGS_DEBUG)
debug: LDFLAGS += $(LDFLAGS_DEBUG)
debug: $(DEBUG_DIR)/$(TARGET_EXEC)

$(RELEASE_DIR)/$(TARGET_EXEC): $(OBJS_RELEASE)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	cp $@ $(TARGET_EXEC)
	wasm-opt $@ -o $@ -Oz && wasm-strip $@

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/$(TARGET_EXEC): $(OBJS_DEBUG)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	../../emscripten/tools/wasm-sourcemap.py $@ -w $@ -p $(CURDIR) -s -u ./$@.map -o $@.map --dwarfdump=/usr/bin/llvm-dwarfdump
	cp $@ $(TARGET_EXEC)
	cp $@.map $(TARGET_EXEC).map

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_EXEC)

-include $(DEPS_DEBUG)
-include $(DEPS_RELEASE)
