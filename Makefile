TARGET = GTRemastered
OBJS = main.o exports.o

CFLAGS = -O2 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

# these do not seem to be available anymore..?
#USE_KERNEL_LIBC = 0
#USE_PSPSDK_LIBC = 1
#USE_KERNEL_LIBS = 1

# use built-in libc to supply sprintf for ppsspp and atoi in general
LIBS = -lc -lpspsystemctrl_kernel

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
