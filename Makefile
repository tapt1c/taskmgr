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
OBJECTS = build/main.o build/system.o build/runtime.o build/taskmgr.res.o build/i18n.o build/performance.o build/driver.o build/process_policy.o build/applications.o build/columns.o build/owner.o build/processes.o build/process_memory.o

.PHONY: all check clean
all: build/taskmgr95.exe
build:
	mkdir -p build
build/%.o: src/%.c src/system.h src/resource.h src/i18n.h src/applications.h src/columns.h src/owner.h src/processes.h src/process_memory.h src/version.h src/performance.h src/driver.h src/process_policy.h driver/protocol.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/taskmgr.res.o: src/taskmgr.rc src/resource.h src/version.h build/tm95stat.vxd | build
	$(WINDRES) --preprocessor $(CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $(CPPFLAGS) $< -o $@
build/tm95stat.obj: driver/tm95stat.c driver/protocol.h | build
	$(WCC) -q -bt=nt -3s -s -zl -zls -zu -zc -of+ -fo=$@ $<
build/tm95stat.vxd: build/tm95stat.obj driver/tm95stat.lnk tools/fix_vxd.py
	$(WLINK) @$(WATCOM_BIN)/wlink.lnk @driver/tm95stat.lnk
	python3 tools/fix_vxd.py $@
build/taskmgr95.exe: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -ladvapi32
check: build/taskmgr95.exe build/process_policy_test
	python3 tools/check_pe.py --objdump $(OBJDUMP) $<
	python3 tools/check_embedded.py $< build/tm95stat.vxd
	./build/process_policy_test
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
build/wine64/%.o: src/%.c src/system.h src/resource.h src/i18n.h src/applications.h src/columns.h src/owner.h src/processes.h src/process_memory.h src/version.h src/performance.h src/driver.h src/process_policy.h driver/protocol.h | build/wine64
	$(WINE_CC) $(CPPFLAGS) $(filter-out -march=i386,$(CFLAGS)) -c $< -o $@
build/wine64/taskmgr.res.o: src/taskmgr.rc src/resource.h src/version.h build/tm95stat.vxd | build/wine64
	$(WINE_RC) --preprocessor $(WINE_CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $(CPPFLAGS) $< -o $@
build/wine64/taskmgr95-ui.exe: $(UI_OBJECTS) build/wine64/taskmgr.res.o
	$(WINE_CC) -nostdlib -mwindows -Wl,--entry,WinMainCRTStartup $^ -o $@ -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -ladvapi32
build/wine64/gui_smoke.exe: tests/gui_smoke.c src/resource.h build/wine64/fixture.res.o | build/wine64
	$(WINE_CC) -Isrc -Wall -Wextra -Werror $< build/wine64/fixture.res.o -o $@ -lcomctl32 -lgdi32 -ladvapi32
wine64-ui: build/wine64/taskmgr95-ui.exe build/wine64/gui_smoke.exe
wine64-smoke: wine64-ui
	python3 tools/run_wine_smoke.py --wine "$(WINE)"

build/process_policy_test: tests/process_policy_test.c src/process_policy.c src/process_policy.h | build
	cc -std=c89 -Wall -Wextra -Werror -Isrc tests/process_policy_test.c src/process_policy.c -o $@
build/wine64/system_policy_test.exe: tests/system_policy_test.c src/system.c src/system.h src/process_policy.c src/process_policy.h | build/wine64
	$(WINE_CC) $(CPPFLAGS) $(filter-out -march=i386,$(CFLAGS)) tests/system_policy_test.c src/system.c src/process_policy.c -o $@ -ladvapi32

build/wine64/fixture.res.o: tests/fixture.rc tests/fixture.ico | build/wine64
	$(WINE_RC) --preprocessor $(WINE_CC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED $< -o $@
