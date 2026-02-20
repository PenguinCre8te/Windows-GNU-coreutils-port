# Compiler and flags
CC      := clang
CFLAGS  := -O3
RM      := del
RM_LIBS := -lshell32 -ladvapi32 -lshlwapi

# Default architecture and output
OUTPUT  ?= timep.exe

all: $(OUTPUT)

$(OUTPUT): main.c
	$(CC) main.c -o $@ $(CFLAGS) $(RM_LIBS)

clean:
	$(RM) *.exe