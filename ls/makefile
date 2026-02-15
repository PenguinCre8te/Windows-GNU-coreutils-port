CC      := clang
CFLAGS  := -O3 -municode
RM      := del
LS_LIBS := -ladvapi32 -lshlwapi

# Allow ARCH override, e.g. ARCH=aarch64-w64-mingw32
ARCH    ?=
# Allow OUTPUT override, default to ls.exe
OUTPUT  ?= ls.exe

all: $(OUTPUT)

$(OUTPUT): main.c
	$(CC) $(ARCH:%=-target %) main.c -o $@ $(CFLAGS) $(LS_LIBS)

clean:
	$(RM) *.exe