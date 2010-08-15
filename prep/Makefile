CC = $(CROSS_COMPILE)gcc
CFLAGS += -Wall -O2
LDFLAGS = -s -O2
ifdef WIZ
CFLAGS += -DWIZ
TAG = _wiz
endif
ifdef PND
CFLAGS += -DPND
TAG = _pnd
endif

vpath %.c = ../common/

TARGET = ginge_prep$(TAG)
OBJS += main.o host_fb.o cmn.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)
