# Builds runner.g3a: the MicroPython add-in that py2g3a.py stitches scripts onto.
#
# You only need this if you want to rebuild the runner yourself. Requirements:
#   - an sh3eb-elf cross compiler (see build.sh, which builds one)
#   - libfxcg      https://github.com/Jonimoose/libfxcg
#   - mkg3a        https://github.com/tari/mkg3a
#   - MicroPython  https://github.com/micropython/micropython (tested with v1.24.1)
#
#   make MPY=/path/to/micropython FXCGSDK=/path/to/libfxcg MKG3A=/path/to/mkg3a

MPY     ?= ../deps/micropython
FXCGSDK ?= ../deps/libfxcg
MKG3A   ?= mkg3a
PREFIX  ?= sh3eb-elf-

CC      := $(PREFIX)gcc
BUILD   := build

MACHDEP := -mb -m4a-nofpu -mhitachi -nostdlib
CFLAGS  := $(MACHDEP) -Os -std=gnu99 -Wall -Wno-unused-function \
           -ffunction-sections -fdata-sections -fno-common -DNDEBUG -DTARGET_PRIZM=1 \
           -I. -Iinclude -I$(BUILD)/embed -I$(MPY) -I$(FXCGSDK)/include
LDFLAGS := $(MACHDEP) -T prizm_py2g3a.x -Wl,-static -Wl,-gc-sections -Wl,-Map,$(BUILD)/runner.map \
           -L$(FXCGSDK)/lib
LIBS    := -lc -lfxcg -lgcc

# MicroPython core + the extra pieces this port uses.
MPY_SRC := $(filter-out $(MPY)/py/nlrx%.c $(MPY)/py/nlrpowerpc.c $(MPY)/py/nlrmips.c $(MPY)/py/nlrxtensa.c, \
             $(wildcard $(MPY)/py/*.c)) \
           $(MPY)/extmod/modrandom.c \
           $(MPY)/shared/runtime/gchelper_generic.c \
           $(filter-out %/thumb_vfp_sqrt.c, $(wildcard $(MPY)/lib/libm_dbl/*.c))
OWN_SRC := main.c casioplot.c math_extra.c

OBJ := $(patsubst $(MPY)/%.c,$(BUILD)/mpy/%.o,$(MPY_SRC)) $(patsubst %.c,$(BUILD)/%.o,$(OWN_SRC))

GENHDR := $(BUILD)/embed/genhdr/qstrdefs.generated.h

.PHONY: all clean
all: runner.g3a

# Step 1: generate MicroPython's headers (qstrs, module table...) using the
# host compiler, exactly as MicroPython's "embed" port does.
$(GENHDR): mpconfigport.h casio_hal.h genhdr.mk casioplot.c
	$(MAKE) -f genhdr.mk MICROPYTHON_TOP=$(abspath $(MPY)) BUILD=$(BUILD)/embed

# Step 2: cross-compile.
$(BUILD)/mpy/%.o: $(MPY)/%.c $(GENHDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-error -c $< -o $@

$(BUILD)/%.o: %.c $(GENHDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Step 3: link to a flat binary at 0x300000, then wrap it as a g3a.
$(BUILD)/runner.bin: $(OBJ) prizm_py2g3a.x
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

runner.g3a: $(BUILD)/runner.bin
	$(MKG3A) -n basic:"PyRunner" $< $@

clean:
	rm -rf $(BUILD) runner.g3a
