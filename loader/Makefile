ARCH ?= arm
CROSS_COMPILE ?= arm-linux-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb -DLOADER
LDFLAGS += -ggdb
#LDFLAGS += -nodefaultlibs # NYET
ifndef DEBUG
CFLAGS += -O2 -fno-strict-aliasing
LDFLAGS += -O2
endif
ifdef DBG
CFLAGS += -DDBG=$(DBG)
endif

ifeq "$(ARCH)" "ia32"
ARCH = ia32
CFLAGS += -m32
LDFLAGS += -m32
endif
ifeq "$(ARCH)" "arm"
OBJ += syscalls.o emu_arm.o
endif
ifdef PND
CFLAGS += -DPND
TAG = _pnd
endif
ifdef WIZ
CFLAGS += -DWIZ
OBJ += wiz_video_arm.o
TAG = _wiz
endif

vpath %.c = ../common/
vpath %.s = ../common/

TARGET_S = ginge_sloader$(TAG)
TARGET_D = ginge_dyn$(TAG)

OBJ += emu.o host.o host_fb.o cmn.o llibc.o
OBJ_S += $(OBJ) loader.o loader_$(ARCH).o patches.o
OBJ_D += $(OBJ) dl.o

all: $(TARGET_S) $(TARGET_D)

$(TARGET_S): LDFLAGS += -Wl,-T script_$(ARCH).lds
$(TARGET_D): LDFLAGS += -ldl -Wl,--version-script=ginge_dyn.symver
$(TARGET_D): LDFLAGS += -Wl,--no-undefined

$(TARGET_S): $(OBJ_S)
	$(CC) -o $@ $^ -static $(LDFLAGS)

$(TARGET_D): $(OBJ_D)
	$(CC) -o $@ $^ -shared $(LDFLAGS)

# easier to support old toolchains with this
%.o: %.s
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	$(RM) $(TARGET_S) $(TARGET_D) $(OBJ_S) $(OBJ_D)

# basic deps
dl.o patches.o: override.c
*.o: header.h
host_fb.o: libpicofe/linux/fbdev.c
