main:
	mkdir -p build
	gcc -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -I./include/ -L./lib/ -lraylib -lm -o build/main src/main.c
