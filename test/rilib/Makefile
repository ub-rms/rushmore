.PHONY: all clean

LIB_A=rilib.a
LIB_SO=rilib.so

SRC_DIRS=src
SRCS=$(shell find $(SRC_DIRS) -name '*.c')

C_INCLUDE_PATH=include
LD_LIBRARY_PATH=

UNAME_S=$(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HOST_ARCH=linux

	CROSS_COMPILE=arm-linux-gnueabihf-
else ifeq ($(UNAME_S),Darwin)
	HOST_ARCH=darwin

	TOOLCHAIN=$(realpath $(shell pwd)/../prebuilts/gcc/$(HOST_ARCH)-x86/arm/arm-linux-androideabi-4.9)
	NDK=$(realpath $(shell pwd)/../prebuilts/ndk)
	PLATFORM=$(NDK)/current/platforms/android-24/arch-arm/usr
	CROSS_COMPILE=$(TOOLCHAIN)/bin/arm-linux-androideabi-
	CFLAGS+=--sysroot=$(PLATFORM)
else
	$(error host must be either Linux or MacOS)
endif

OBJ_DIR=obj/$(HOST_ARCH)
OBJS=$(addprefix $(OBJ_DIR)/,$(addsuffix .o, $(basename $(SRCS))))

CFLAGS+=-march=armv7-a -Wl,--fix-cortex-a8 -mthumb -mfpu=vfpv3-d16 -O2 -fPIE -fPIC -mfpu=neon
LDFLAGS+=-lpthread
C_INCLUDE_PATH+=$(PLATFORM)/include
LD_LIBRARY_PATH+=$(PLATFORM)/lib

export CC = $(CROSS_COMPILE)gcc
export CXX = $(CROSS_COMPILE)g++
export LD = $(CROSS_COMPILE)ld
export AR = $(CROSS_COMPILE)ar
export RANLIB = $(CROSS_COMPILE)ranlib
export AS = $(CROSS_COMPILE)as
export NM = $(CROSS_COMPILE)nm
export STRIP = $(CROSS_COMPILE)strip
export OBJCOPY = $(CROSS_COMPILE)objcopy
export OBJDUMP = $(CROSS_COMPILE)objdump

cflags-y += -D__ARM_NEON__

# all: $(OBJS)
# 	$(CC) $(CFLAGS) -o $(LIB_A) $^ $(addprefix -L,$(LD_LIBRARY_PATH)) $(LDFLAGS) -pie

all: $(LIB_A) $(LIB_SO)

$(LIB_A): $(OBJS)
	$(AR) rcs $@ $^

$(LIB_SO): $(OBJS)
	$(CC) $(CFLAGS) -shared -o $(LIB_SO) $^

$(OBJ_DIR)/%.o: %.c $(CC) $(PLATFORM)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(addprefix -I,$(C_INCLUDE_PATH)) -c -o $@ $<

$(CC):
	git submodule update --init --recursive -- $(TOOLCHAIN)

$(PLATFORM):
	git submodule update --init --recursive -- $(NDK)

clean:
	rm -f $(LIB_A) $(LIB_SO)
	rm -rf $(OBJ_DIR)
