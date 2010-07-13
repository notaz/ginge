ARCH ?= arm
CROSS_COMPILE ?= arm-linux-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb
LDFLAGS += -static -ggdb
ifndef DEBUG
CFLAGS += -O2 -fno-strict-aliasing
LDFLAGS += -s -O2
endif

ifeq "$(ARCH)" "ia32"
ARCH = ia32
CFLAGS += -m32
LDFLAGS += -m32
ASFLAGS += --32
endif
ifeq "$(ARCH)" "arm"
ASFLAGS += -mfloat-abi=soft
OBJ += sys_cacheflush.o
endif

vpath %.c = ../common/

OBJ += loader.o loader_$(ARCH).o patches.o emu.o host_fb.o host_pnd.o

ginge_sloader: LDFLAGS += -Wl,-T script_$(ARCH).lds

ginge_sloader: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) loader $(OBJ)
