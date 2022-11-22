LIB_PATH = raylib-4.2.0_linux_amd64/lib
INC_PATH = raylib-4.2.0_linux_amd64/include

CFLAGS = -Wall -Wextra -L$(LIB_PATH) -I$(INC_PATH) -g #-fsanitize=address
LFLAGS = -l:libraylib.a -lm #-fsanitize=address

all: snbpad

snbpad: sfd.c textrenderutils.c treeview.c guielement.c snbpad.c gap.c gapiter.c textdisplay.c splitview.c xutf8.c
	gcc $^ -o $@ $(CFLAGS) $(LFLAGS)

clean:
	rm snbpad