CC = clang

UNAME_S := $(shell uname -s)
INCLUDES = -I./include/ $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample libswscale)
LDFLAGS  = $(shell pkg-config --libs libavformat libavcodec libavutil libswresample libswscale) -lm
ifeq ($(UNAME_S),Darwin)
    CFLAGS = -ggdb -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -L./lib/macos -lraylib
else
    CFLAGS = -ggdb -Wall -Wextra -pedantic -lGL -lm -lpthread -ldl -lrt -lX11 -L./lib/linux -l:libraylib.a
endif


bytestream: generator
	./generator
	mkdir -p build
	$(CC) -o build/bytestream src/*.c $(CFLAGS) $(INCLUDES) $(LDFLAGS) 

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
