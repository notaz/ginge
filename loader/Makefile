ARCH ?= arm
CROSS_COMPILE ?= arm-linux-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb -DLOADER
LDFLAGS += -ggdb -lpthread -lrt
ifndef DEBUG
CFLAGS += -O2 -fno-strict-aliasing
LDFLAGS += -s -O2
endif
ifdef DBG
CFLAGS += -DDBG=$(DBG)
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

TARGET_S = ginge_sloader
TARGET_D = ginge_dyn

OBJ += emu.o host_fb.o host_pnd.o cmn.o
OBJ_S += $(OBJ) loader.o loader_$(ARCH).o patches.o
OBJ_D += $(OBJ) dl.o

all: $(TARGET_S) $(TARGET_D)

$(TARGET_S): LDFLAGS += -Wl,-T script_$(ARCH).lds
$(TARGET_D): LDFLAGS += -ldl

$(TARGET_S): $(OBJ_S)
	$(CC) -o $@ $^ -static $(LDFLAGS)

$(TARGET_D): $(OBJ_D)
	$(CC) -o $@ $^ -shared $(LDFLAGS)

clean:
	$(RM) $(TARGET_S) $(TARGET_D) $(OBJ_S) $(OBJ_D)

# basic deps
dl.o patches.o: override.c
*.o: header.h

