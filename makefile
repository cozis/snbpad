LIB_PATH = raylib-4.2.0_linux_amd64/lib
INC_PATH = raylib-4.2.0_linux_amd64/include

CFLAGS = -Wall -Wextra -L$(LIB_PATH) -I$(INC_PATH)
LFLAGS = -l:libraylib.a -lm

all: snbpad

snbpad: snbpad.c gap.c gapiter.c textdisplay.c xutf8.c
	gcc $^ -o $@ $(CFLAGS) $(LFLAGS)

clean:
	rm snbpad