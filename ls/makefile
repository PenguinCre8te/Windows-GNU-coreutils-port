# Compiler and flags
CC      := clang
CFLAGS  := -O3 -municode
RM      := del
RM_LIBS := -lshell32 -ladvapi32 -lshlwapi

# Default architecture and output
OUTPUT  ?= ls.exe

all: $(OUTPUT)

$(OUTPUT): main.c
	$(CC) main.c -o $@ $(CFLAGS) $(RM_LIBS)

clean:
	$(RM) *.exe