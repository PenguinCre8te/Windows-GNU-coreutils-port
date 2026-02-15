# Compiler and flags
CC      := clang
CFLAGS  := -O3 -municode
RM      := del
RM_LIBS := -lshell32 -ladvapi32 -lshlwapi

# Default architecture and output
ARCH    ?= x86_64-w64-mingw32
OUTPUT  ?= rm.exe

# Sysroot: if LLVM_MINGW is set, use it; otherwise rely on system defaults
SYSROOT ?= $(LLVM_MINGW)\$(ARCH)

all: $(OUTPUT)

$(OUTPUT): main.c
ifeq ($(SYSROOT),)
	$(CC) --target=$(ARCH) main.c -o $@ $(CFLAGS) $(RM_LIBS)
else
	$(CC) --target=$(ARCH) --sysroot=$(SYSROOT) main.c -o $@ $(CFLAGS) $(RM_LIBS)
endif

clean:
	$(RM) *.exe