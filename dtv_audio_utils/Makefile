M_PREFIX=$(STAGING_DIR)/usr
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include
AML_BUILD_DIR?=.

TARGET_SRCS=utils/tsp_utils.c \
    utils/tsplinux.c \
    audio_data_read/uio_audio_read.c \
    avutils/amthreadpool.c \
    avutils/itemlist.c \
    avutils/amsysfsutils.c \
    avutils/amconfigutils.c

TARGET_SRCPPS=utils/Unicode.cpp \
    utils/TSPHandler.cpp \
    utils/TSPLooper.cpp \
    utils/TSPLooperRoster.cpp \
    utils/TSPMessage.cpp \
    utils/RefBase.cpp \
    utils/SharedBuffer.cpp \
    utils/String16.cpp \
    utils/String8.cpp \
    utils/StrongPointer.cpp \
    utils/Threads.cpp \
    utils/Timers.cpp \
    utils/VectorImpl.cpp \
    dmxwrap/AmDemuxWrapper.cpp \
    dmxwrap/HwDemux/AmHwDemuxWrapper.cpp \
    dmxwrap/MultiHwDemux/AmHwMultiDemuxWrapper.cpp \
    dmxwrap/MultiHwDemux/AmLinuxDvb.cpp \
    dmxwrap/MultiHwDemux/AmDmx.cpp \
    audio_data_read/dmx_audio_es.cpp


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

TARGET_OBJS = $(patsubst %.c, $(AML_BUILD_DIR)/%.o, $(notdir $(TARGET_SRCS)))
TARGET_OBJS += $(patsubst %.cpp, $(AML_BUILD_DIR)/%.o, $(notdir $(TARGET_SRCPPS)))
SRCS_DIR = $(dir $(TARGET_SRCS) $(TARGET_SRCPPS))

vpath %.c $(SRCS_DIR)

$(AML_BUILD_DIR)/%.o:%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

vpath %.cpp $(SRCS_DIR)

$(AML_BUILD_DIR)/%.o:%.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)


all: $(AML_BUILD_DIR)/$(TARGET)

$(AML_BUILD_DIR)/$(TARGET): $(TARGET_OBJS)
	$(CXX) -o $@ $^ -shared -fPIC $(CFLAGS)

install:
	-install -m 555 $(AML_BUILD_DIR)/${TARGET} $(INSTALL_DIR)
	-install -m 555 $(AML_BUILD_DIR)/$(TARGET) $(TARGET_DIR)/usr/lib
	install -d  $(HEADERS_DIR)/audio_read_api
	install -d  $(HEADERS_DIR)/sync
	cp -rf $(SRC)/audio_read_api/*.h  $(HEADERS_DIR)/
	cp -rf $(SRC)/sync/*.h  $(HEADERS_DIR)/
force:

clean:
	-rm -rf $(AML_BUILD_DIR)/*.o $(AML_BUILD_DIR)/libdvbaudioutils.so
