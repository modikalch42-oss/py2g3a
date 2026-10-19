# Generates MicroPython's headers for this port (run on the host).
# Invoked by the main Makefile; MICROPYTHON_TOP and BUILD are passed in.

# modrandom.c lives in extmod/, which the embed port doesn't scan by default.
SRC_QSTR += extmod/modrandom.c

include $(MICROPYTHON_TOP)/py/mkenv.mk
include $(TOP)/py/py.mk

# The calculator is big-endian; make sure qstrs like "big" are generated.
CFLAGS += -I. -I$(TOP) -I$(BUILD) -std=gnu99 $(ENDIAN)
ENDIAN ?= -DMP_ENDIANNESS_LITTLE=0

all: $(BUILD)/genhdr/qstrdefs.generated.h $(BUILD)/genhdr/moduledefs.h \
     $(BUILD)/genhdr/root_pointers.h $(BUILD)/genhdr/mpversion.h

include $(TOP)/py/mkrules.mk
