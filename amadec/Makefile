M_PREFIX=$(STAGING_DIR)/usr
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

TARGET_OBJS=adec-external-ctrl.o \
	adec-internal-mgt.o \
	adec-ffmpeg-mgt.o \
	adec-message.o \
	adec-pts-mgt.o \
	adec_write.o \
	adec_read.o \
	adec_omx_brige.o \
	audio_out/dtv_patch_out.o \
	audio_out/aml_resample.o \
	audiodsp_update_format.o

TARGET=libaudamadec.so

INSTALL_DIR=${LIB_DIR}

#$(warning  AML_AUDIO_HAL_STAGING_DIR is ${AML_AUDIO_HAL_STAGING_DIR})
#CFLAGS += -shared -lpthread -lm -lrt -llog -lcutils -L$ENV{AML_AUDIO_HAL_STAGING_DIR}/usr/lib/
#-lasound
CFLAGS += -shared -lpthread -lm -lasound -lrt -DBUILD_LINUX

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC -g -D_POSIX_SOURCE -DDOLBY_USE_ARMDEC -DUSE_ARM_AUDIO_DEC -DUSE_AOUT_IN_ADEC
#CFLAGS+=-I$(SRC)/dmxwrap -I$(SRC)/include -I$(SRC)/utils -I$(SRC)/../utils/include -I$(SRC)/../include -I$(SRC)/../../libplayer/src/amavutils/include -I$(SRC)/../../../vendor/amlogic/dvb/include/am_adp -I$(SRC)/dmxwrap/HwDemux -I$(SRC)/dmxwrap/MultiHwDemux -I$(SRC)/audio_read_api -I$(SRC)/sync -L${LIB_DIR} -I$(HEADERS_DIR)
CFLAGS+=-I$(SRC)/include -I$(SRC) -I$(SRC)/../include -I$(SRC)/../../libplayer/src/amavutils/include

%.o:%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o:%.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)


all: $(TARGET)

$(TARGET): $(TARGET_OBJS)
	$(CXX) -o $@ $^ -shared -fPIC $(CFLAGS)

#install:
	#-install -m 555 ${TARGET} $(INSTALL_DIR)
	#-install -m 555 $(TARGET) $(TARGET_DIR)/usr/lib
	#install -d  $(HEADERS_DIR)/audio_read_api
	#install -d  $(HEADERS_DIR)/sync
	#cp -rf $(SRC)/audio_read_api/*.h  $(HEADERS_DIR)/
	#cp -rf $(SRC)/sync/*.h  $(HEADERS_DIR)/
force:

clean:
	-rm -rf *.o libaudamadec.so
