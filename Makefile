CC = clang

UNAME_S := $(shell uname -s)
INCLUDES = -I./include/ $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample libswscale)
LDFLAGS  = $(shell pkg-config --libs libavformat libavcodec libavutil libswresample libswscale) -lm
ifeq ($(UNAME_S),Darwin)
    CFLAGS = -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -L./lib/macos -lraylib
else ifeq ($(OS),Windows_NT)
    CFLAGS = -mwindows -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/windows -l:libraylib.a
else
    CFLAGS = -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/linux -l:libraylib.a
endif


bytestream: generator
	./generator
	$(CC) -o build/bytestream src/*.c $(CFLAGS) $(INCLUDES) $(LDFLAGS)

release: generator assets/macos/bytestream.icns
	./generator
	mkdir -p build
ifeq ($(UNAME_S),Darwin)
	$(CC) -o build/bytestream src/*.c -O2 -DNDEBUG -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -L./lib/macos -lraylib $(INCLUDES) $(LDFLAGS)
	mkdir -p build/bytestream.app/Contents/MacOS
	mkdir -p build/bytestream.app/Contents/Resources
	mv build/bytestream build/bytestream.app/Contents/MacOS/
	cp assets/macos/bytestream.icns build/bytestream.app/Contents/Resources/bytestream.icns
	cp assets/macos/Info.plist build/bytestream.app/Contents/Info.plist
else ifeq ($(OS),Windows_NT)
	$(CC) -o build/bytestream.exe src/*.c -O2 -DNDEBUG -mwindows -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/windows -l:libraylib.a $(INCLUDES) $(LDFLAGS)
else
	$(CC) -o build/bytestream src/*.c -O2 -DNDEBUG -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/linux -l:libraylib.a $(INCLUDES) $(LDFLAGS)
endif

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

generator:
	cc generator.c -o ./generator

test:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) test.c -o test

video:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) video.c -o test

ui:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) ui.c -o ui && ./ui

clean:
	rm -rf build test ui
