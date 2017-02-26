TARGET = pmodoled
C_SRCS += pmodoled.c
CFLAGS += -O2 -fno-builtin-printf

BSP_BASE = ../../bsp
include $(BSP_BASE)/env/common.mk
