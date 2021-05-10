srcs-y += chacha20_vec.c
#srcs-y += chacha20.c


cflags-y += -D__ARM_NEON__
cflags-y += -mfpu=neon
