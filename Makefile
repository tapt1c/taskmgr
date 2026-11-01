CROSS ?= i686-w64-mingw32-
CC = $(CROSS)gcc
WINDRES = $(CROSS)windres
OBJDUMP = $(CROSS)objdump
CPPFLAGS = -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0400 -Isrc
CFLAGS = -std=c89 -Os -Wall -Wextra -Werror -Wno-cast-function-type -march=i386 -fno-builtin -fno-stack-protector -fno-ident
LDFLAGS = -nostdlib -mwindows -Wl,--entry,_WinMainCRTStartup@0 -Wl,--subsystem,windows:4.0 -Wl,--major-os-version,4,--minor-os-version,0 -Wl,--disable-dynamicbase,--disable-nxcompat
OBJECTS = build/main.o build/system.o build/runtime.o build/taskmgr.res.o

.PHONY: all check clean
all: build/taskmgr95.exe
build:
	mkdir -p build
build/%.o: src/%.c src/system.h src/resource.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/taskmgr.res.o: src/taskmgr.rc src/resource.h | build
	$(WINDRES) --preprocessor $(CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $(CPPFLAGS) $< -o $@
build/taskmgr95.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ -lcomctl32 -lgdi32 -luser32 -lkernel32
check: build/taskmgr95.exe
	python3 tools/check_pe.py --objdump $(OBJDUMP) $<
clean:
	rm -rf build
