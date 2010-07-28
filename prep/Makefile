CC = $(CROSS_COMPILE)gcc
CFLAGS += -Wall -O2
LDFLAGS = -s -O2

vpath %.c = ../common/

TARGET = ginge_prep
OBJS = main.o host_fb.o cmn.o

all: $(TARGET)

ginge_prep: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)
