M_PREFIX=$(STAGING_DIR)/usr
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

TARGET_OBJS= utils/tsp_utils.o \
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
    audio_data_read/dmx_audio_es.o \
    audio_data_read/uio_audio_read.o \
    sync/audio_dtv_sync.o \
    avutils/amthreadpool.o \
    avutils/itemlist.o \
    avutils/amsysfsutils.o \
    avutils/amconfigutils.o

TARGET=libdvbaudioutils.so

INSTALL_DIR=${LIB_DIR}

CFLAGS += -shared -lpthread -lm -lrt -llog -lcutils -DBUILD_LINUX

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC -g
CFLAGS+=-I$(SRC)/dmxwrap -I$(SRC)/include -I$(SRC)/utils -I$(SRC)/../utils/include -I$(SRC)/../include \
	-I$(SRC)/dmxwrap/HwDemux -I$(SRC)/dmxwrap/MultiHwDemux -I$(SRC)/audio_read_api \
	-I$(SRC)/sync -I$(SRC)/avutils
CFLAGS+=-I$(SRC)/../include/basic_utils

%.o:%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o:%.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)


all: $(TARGET)

$(TARGET): $(TARGET_OBJS)
	$(CXX) -o $@ $^ -shared -fPIC $(CFLAGS)

install:
	-install -m 555 ${TARGET} $(INSTALL_DIR)
	-install -m 555 $(TARGET) $(TARGET_DIR)/usr/lib
	install -d  $(HEADERS_DIR)/audio_read_api
	install -d  $(HEADERS_DIR)/sync
	cp -rf $(SRC)/audio_read_api/*.h  $(HEADERS_DIR)/
	cp -rf $(SRC)/sync/*.h  $(HEADERS_DIR)/
force:

clean:
	-rm -rf *.o libdvbaudioutils.so
