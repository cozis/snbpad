#-fsanitize=address -g

CFLAGS = -Wall -Wextra
LFLAGS = -Wl,-rpath,.

all: snbpad

snbpad: libSDL2_ttf.so libSDL2-2.0.so snbpad.c gap.c gapiter.c textdisplay.c xutf8.c
	gcc $^ -o $@ $(CFLAGS) $(LFLAGS)

clean:
	rm snbpad