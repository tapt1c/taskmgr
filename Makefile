CROSS ?= i686-w64-mingw32-
CC = $(CROSS)gcc
WINDRES = $(CROSS)windres
OBJDUMP = $(CROSS)objdump
WATCOM_BIN ?= /workspace/toolchains/watcom/root/binl64
WCC = $(WATCOM_BIN)/wcc386
WLINK = $(WATCOM_BIN)/wlink
CPPFLAGS = -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0400 -Isrc
CFLAGS = -std=c89 -Os -Wall -Wextra -Werror -Wno-cast-function-type -march=i386 -fno-builtin -fno-stack-protector -fno-ident
LDFLAGS = -nostdlib -mwindows -Wl,--entry,_WinMainCRTStartup@0 -Wl,--subsystem,windows:4.0 -Wl,--major-os-version,4,--minor-os-version,0 -Wl,--disable-dynamicbase,--disable-nxcompat
OBJECTS = build/main.o build/system.o build/runtime.o build/taskmgr.res.o build/i18n.o build/performance.o build/driver.o

.PHONY: all check clean
all: build/taskmgr95.exe
build:
	mkdir -p build
build/%.o: src/%.c src/system.h src/resource.h src/i18n.h src/performance.h src/driver.h driver/protocol.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/taskmgr.res.o: src/taskmgr.rc src/resource.h build/tm95stat.vxd | build
	$(WINDRES) --preprocessor $(CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $(CPPFLAGS) $< -o $@
build/tm95stat.obj: driver/tm95stat.c driver/protocol.h | build
	$(WCC) -q -bt=nt -3s -s -zl -zls -zu -zc -of+ -fo=$@ $<
build/tm95stat.vxd: build/tm95stat.obj driver/tm95stat.lnk tools/fix_vxd.py
	$(WLINK) @$(WATCOM_BIN)/wlink.lnk @driver/tm95stat.lnk
	python3 tools/fix_vxd.py $@
build/taskmgr95.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -ladvapi32
check: build/taskmgr95.exe
	python3 tools/check_pe.py --objdump $(OBJDUMP) $<
	python3 tools/check_embedded.py $< build/tm95stat.vxd
clean:
	rm -rf build

# Additional UI test build only; never distribute this as the Win95 executable.
WINE_CC ?= x86_64-w64-mingw32-gcc
WINE_RC ?= x86_64-w64-mingw32-windres
WINE ?= wine
UI_OBJECTS = $(addprefix build/wine64/,$(notdir $(filter-out build/taskmgr.res.o,$(OBJECTS))))
.PHONY: wine64-ui wine64-smoke
build/wine64:
	mkdir -p $@
build/wine64/%.o: src/%.c src/system.h src/resource.h src/i18n.h src/performance.h src/driver.h driver/protocol.h | build/wine64
	$(WINE_CC) $(CPPFLAGS) $(filter-out -march=i386,$(CFLAGS)) -c $< -o $@
build/wine64/taskmgr.res.o: src/taskmgr.rc src/resource.h build/tm95stat.vxd | build/wine64
	$(WINE_RC) --preprocessor $(WINE_CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $(CPPFLAGS) $< -o $@
build/wine64/taskmgr95-ui.exe: $(UI_OBJECTS) build/wine64/taskmgr.res.o
	$(WINE_CC) -nostdlib -mwindows -Wl,--entry,WinMainCRTStartup $^ -o $@ -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -ladvapi32
build/wine64/gui_smoke.exe: tests/gui_smoke.c src/resource.h | build/wine64
	$(WINE_CC) -Isrc -Wall -Wextra -Werror $< -o $@ -lcomctl32
wine64-ui: build/wine64/taskmgr95-ui.exe build/wine64/gui_smoke.exe
wine64-smoke: wine64-ui
	$(WINE) build/wine64/gui_smoke.exe 'Z:$(subst /,\,$(CURDIR))/build/wine64/taskmgr95-ui.exe'
