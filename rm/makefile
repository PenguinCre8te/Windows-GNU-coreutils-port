CC      := clang
CFLAGS  := -O3 -municode
RM      := del
RM_LIBS := -lshell32

# Allow ARCH override, e.g. ARCH=x86_64-w64-mingw32
ARCH    ?=
# Allow OUTPUT override, default to rm.exe
OUTPUT  ?= rm.exe

all: $(OUTPUT)

$(OUTPUT): main.c
	$(CC) $(ARCH:%=-target %) main.c -o $@ $(CFLAGS) $(RM_LIBS)

clean:
	$(RM) *.exe