.PHONY: all clean

SRC_DIRS=rilib rand vcrypto
OBJ_DIR=obj
SRCS=$(shell find $(SRC_DIRS) -name '*.c')
OBJS=$(addprefix $(OBJ_DIR)/,$(addsuffix .o, $(basename $(SRCS))))

C_INCLUDE_DIR+=include
TEST_DIR=test
CROSS_COMPILE=arm-linux-gnueabihf-
CC=gcc
CFLAGS=-O2 -mfpu=neon

all: $(OBJS)
	$(CROSS_COMPILE)ar rcs libvcrypto.a $^

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(addprefix -I, $(C_INCLUDE_DIR)) -c -o $@ $<

test: $(OBJS) test.c
	$(CC) $(CFLAGS) -I $(C_INCLUDE_DIR) $^ -o test.elf

clean:
	rm -f libvcrypto.a
	rm -rf $(OBJ_DIR)
