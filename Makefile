EMCC ?= emcc
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
BUILD_DIR := build
WEB_OUTPUT := web/betis_mode.js
CORE_SOURCES := src/betis_mode.c src/audio_synth.c

.PHONY: all firmware wasm test clean

all: firmware

firmware:
	powershell -NoProfile -ExecutionPolicy Bypass -File Build.ps1

wasm: $(WEB_OUTPUT)

$(WEB_OUTPUT): $(CORE_SOURCES) src/wasm_bridge.c src/betis_mode.h src/audio_synth.h
	$(EMCC) $(CFLAGS) $(CORE_SOURCES) src/wasm_bridge.c -I src \
		-s WASM=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXPORTED_FUNCTIONS="['_malloc','_free','_betis_wasm_get_framebuffer','_betis_wasm_init','_betis_wasm_set_sample_rate','_betis_wasm_tick','_betis_wasm_set_knob','_betis_wasm_set_bpm','_betis_wasm_set_arpeggiator','_betis_wasm_note_on','_betis_wasm_note_off','_betis_wasm_all_notes_off','_betis_wasm_render_audio']" \
		-s EXPORTED_RUNTIME_METHODS="['HEAPU8','HEAPF32']" \
		-o $(WEB_OUTPUT)

test: $(BUILD_DIR)/test_betis
	$(BUILD_DIR)/test_betis

$(BUILD_DIR)/test_betis: $(CORE_SOURCES) src/wasm_bridge.c tests/test_betis.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -UNDEBUG -I src $(CORE_SOURCES) src/wasm_bridge.c tests/test_betis.c -lm -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f web/betis_mode.js web/betis_mode.wasm
