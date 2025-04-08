CC = clang

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)  # macOS
    CFLAGS = -ggdb -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
else  # Non-macOS (e.g., Linux)
    CFLAGS = -ggdb -Wall -Wextra -pedantic
endif

INCLUDES = -I./include/ $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample libswscale)
LDFLAGS = -L./lib/ -lraylib -lm $(shell pkg-config --libs libavformat libavcodec libavutil libswresample libswscale)

# Targets
avp:
	mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o build/avp src/*.c

test:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) test.c -o test

video:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) video.c -o test

ui:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) ui.c -o ui && ./ui

clean:
	rm -rf build test ui
