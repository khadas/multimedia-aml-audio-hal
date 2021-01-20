M_PREFIX=$(STAGING_DIR)/usr
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

TARGET_OBJS=    utils/tsp_utils.o \
    utils/tsplinux.o \
    utils/Unicode.o \
    utils/TSPHandler.o \
    utils/TSPLooper.o \
    utils/TSPLooperRoster.o \
    utils/TSPMessage.o \
    utils/RefBase.o \
    utils/SharedBuffer.o \
    utils/String16.o \
    utils/String8.o \
    utils/StrongPointer.o \
    utils/Threads.o \
    utils/Timers.o \
    utils/VectorImpl.o \
    dmxwrap/AmDemuxWrapper.o \
    dmxwrap/HwDemux/AmHwDemuxWrapper.o \
    dmxwrap/MultiHwDemux/AmHwMultiDemuxWrapper.o \
    dmxwrap/MultiHwDemux/AmLinuxDvb.o \
    dmxwrap/MultiHwDemux/AmDmx.o \
    audio_read_api/audio_es.o \
    sync/audio_dtv_sync.o

TARGET=libdvbaudioutils.so

INSTALL_DIR=${LIB_DIR}

CFLAGS += -shared -lpthread -lm -lrt

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC -g
CFLAGS+=-I$(SRC)/dmxwrap -I$(SRC)/include -I$(SRC)/utils -I$(SRC)/dmxwrap/HwDemux -I$(SRC)/dmxwrap/MultiHwDemux -I$(SRC)/audio_read_api -I$(SRC)/sync -L${LIB_DIR} -I$(HEADERS_DIR)

%.o:%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o:%.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)


all: $(TARGET)

$(TARGET): $(TARGET_OBJS)
	$(CXX) -o $@ $^ -shared -fPIC

install:
	-install -m 555 ${TARGET} $(INSTALL_DIR)
	-install -m 555 $(TARGET) $(TARGET_DIR)/usr/lib
	cp -rf $(SRC)/include/*  $(HEADERS_DIR)/

force:

clean:
	-rm -rf *.o libdvbaudioutils.so
