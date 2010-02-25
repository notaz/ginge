CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb
LDFLAGS += -static -ggdb

ifndef ARCH
ARCH = ia32
CFLAGS += -m32
LDFLAGS += -m32
ASFLAGS += --32
endif
ifeq "$(ARCH)" "arm"
ASFLAGS += -mfloat-abi=soft
OBJ += sys_cacheflush.o
endif

OBJ += loader.o loader_$(ARCH).o patches.o emu.o

loader: $(OBJ)

loader: LDFLAGS += -Wl,-T script_$(ARCH).lds

clean:
	$(RM) loader $(OBJ)
