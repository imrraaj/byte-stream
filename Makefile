CC = clang

UNAME_S := $(shell uname -s)
INCLUDES = -I./include/ $(addprefix -I./external/,$(notdir $(wildcard external/*))) $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample libswscale)
LDFLAGS  = $(shell pkg-config --libs libavformat libavcodec libavutil libswresample libswscale) -lm

# Source files
SRCS = $(wildcard src/*.c)
EXTERNAL_SRCS = $(wildcard external/*/*.c)
EXTERNAL_OBJS = $(patsubst external/%.c,build/external_%.o,$(EXTERNAL_SRCS))

# Platform-specific flags
ifeq ($(UNAME_S),Darwin)
    COMPILE_FLAGS = -Wall -Wextra -pedantic
    PLATFORM_LDFLAGS = -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -L./lib/macos -lraylib
else ifeq ($(OS),Windows_NT)
    COMPILE_FLAGS = -Wall -Wextra -pedantic -mwindows
    PLATFORM_LDFLAGS = -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/windows -l:libraylib.a
else
    COMPILE_FLAGS = # -Wall -Wextra -pedantic
    PLATFORM_LDFLAGS = -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/linux -l:libraylib.a
endif

# Combined flags
CFLAGS = $(COMPILE_FLAGS)
ALL_LDFLAGS = $(PLATFORM_LDFLAGS) $(LDFLAGS)

# Main target - compile main sources + link with external objects
build/bytestream: include/bundle.h $(SRCS) $(EXTERNAL_OBJS) | build
	$(CC) -o $@ $(SRCS) $(EXTERNAL_OBJS) $(CFLAGS) $(INCLUDES) $(ALL_LDFLAGS)

# Generic rule for compiling external dependencies
build/external_%.o: external/%.c | build
	@echo "Compiling external dependency: $<"
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDES)



bytestream: build/bytestream

# Ensure build directory exists
build:
	mkdir -p build

# Bundle header generation
include/bundle.h: generator assets/*
	./generator

# Platform-specific release targets
release: include/bundle.h
	mkdir -p build
ifeq ($(UNAME_S),Darwin)
	$(MAKE) release-macos
else ifeq ($(OS),Windows_NT)
	$(MAKE) release-windows
else
	$(MAKE) release-linux
endif

# Generic rule for compiling external dependencies for release
build/external_%-release.o: external/%.c | build
	@echo "Building external dependency for release: $<"
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ -O2 -DNDEBUG $(COMPILE_FLAGS) $(INCLUDES)

EXTERNAL_RELEASE_OBJS = $(patsubst external/%.c,build/external_%-release.o,$(EXTERNAL_SRCS))

# macOS release with app bundle
release-macos: include/bundle.h assets/macos/bytestream.icns $(EXTERNAL_RELEASE_OBJS)
	$(CC) -o build/bytestream $(SRCS) $(EXTERNAL_RELEASE_OBJS) -O2 -DNDEBUG $(INCLUDES) -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -L./lib/macos -lraylib $(LDFLAGS)
	mkdir -p build/bytestream.app/Contents/MacOS
	mkdir -p build/bytestream.app/Contents/Resources
	mv build/bytestream build/bytestream.app/Contents/MacOS/
	cp assets/macos/bytestream.icns build/bytestream.app/Contents/Resources/bytestream.icns
	cp assets/macos/Info.plist build/bytestream.app/Contents/Info.plist
	@echo "macOS app bundle created at build/bytestream.app"

# Windows release
release-windows: include/bundle.h $(EXTERNAL_RELEASE_OBJS)
	$(CC) -o build/bytestream.exe $(SRCS) $(EXTERNAL_RELEASE_OBJS) -O2 -DNDEBUG $(INCLUDES) -mwindows -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/windows -l:libraylib.a $(LDFLAGS)
	@echo "Windows executable created at build/bytestream.exe"

# Linux release
release-linux: include/bundle.h $(EXTERNAL_RELEASE_OBJS)
	$(CC) -o build/bytestream $(SRCS) $(EXTERNAL_RELEASE_OBJS) -O2 -DNDEBUG $(INCLUDES) -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/linux -l:libraylib.a $(LDFLAGS)
	@echo "Linux executable created at build/bytestream"

# Minimal icon sizes (only what's actually needed)
ICON_SIZES = 16 32 128 256 512

# Regenerate macOS icon only when logo changes
assets/macos/bytestream.icns: assets/logos/bytestream-bg.png
	@echo "Regenerating macOS icon..."
	mkdir -p build/icon.iconset
	$(foreach size,$(ICON_SIZES),sips -z $(size) $(size) $< --out build/icon.iconset/icon_$(size)x$(size).png;)
	$(foreach size,16 32,sips -z $(shell echo $$(($(size)*2))) $(shell echo $$(($(size)*2))) $< --out build/icon.iconset/icon_$(size)x$(size)@2x.png;)
	iconutil -c icns build/icon.iconset --output $@
	rm -rf build/icon.iconset
	@echo "Icon updated!"

# Generator for bundling assets
generator: generator.c
	cc generator.c -o ./generator

test: include/bundle.h
	$(CC) test.c -o test $(CFLAGS) $(INCLUDES) $(ALL_LDFLAGS)

video: include/bundle.h
	$(CC) video.c -o test $(CFLAGS) $(INCLUDES) $(ALL_LDFLAGS)

ui: include/bundle.h
	$(CC) ui.c -o ui $(CFLAGS) $(INCLUDES) $(ALL_LDFLAGS) && ./ui

clean:
	rm -rf build test ui generator include/bundle.h

# Clean only main sources (keep tinyfiledialogs object)
clean-fast:
	rm -f build/bytestream generator include/bundle.h

# Phony targets
.PHONY: bytestream release release-macos release-windows release-linux clean clean-fast test video ui
