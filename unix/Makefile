# --- Tool specific options ------------------------------------------------

ifeq ($(DESKTOP),IPHONE)

ifeq ($(IPHONE_TOOLS_DIR),)
IPHONE_TOOLS_DIR=/usr
endif

CC=$(IPHONE_TOOLS_DIR)/bin/arm-apple-darwin9-gcc
AR=$(IPHONE_TOOLS_DIR)/bin/arm-apple-darwin9-ar
RANLIB = $(IPHONE_TOOLS_DIR)/bin/arm-apple-darwin9-ranlib

SOUNDSRC = 

else

RANLIB = ranlib

SOUNDSRC = roadmap_sound.c

endif


HOST=`uname -s`
ifeq ($(HOST),Darwin)
	ARFLAGS="r"
else
	ARFLAGS="r"
endif


# --- RoadMap sources & targets --------------------------------------------

OSLIBSRCSCORE=roadmap_file.c \
              roadmap_path.c \
              roadmap_library.c \
              roadmap_net.c \
              ../md5.c \
              roadmap_serial.c \
              roadmap_spawn.c \
              roadmap_device_events.c \
              roadmap_device.c \
              roadmap_input_type.c \
              roadmap_time.c

OSLIBSRCS=$(OSLIBSRCSCORE) $(SOUNDSRC)
OSLIBOBJS=$(OSLIBSRCS:.c=.o)


# --- Conventional targets ----------------------------------------

all: runtime

build:

runtime: libosroadmap.a

clean: cleanone

cleanone:
	rm -f *.o *.a *.da

install:

uninstall:


# --- The real targets --------------------------------------------

libosroadmap.a: $(OSLIBOBJS)
	$(AR) $(ARFLAGS) libosroadmap.a $(OSLIBOBJS)
	$(RANLIB) libosroadmap.a

