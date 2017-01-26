# Makefile

# Compilation options.

ARCH     ?= x86_64
SDK      ?= macosx
CORE_DIR ?= core
CORE_LIB ?= $(CORE_DIR)/libmemctl_core.a

ifneq ($(ARCH),x86_64)
CLANG   := $(shell xcrun --sdk $(SDK) --find clang)
AR      := $(shell xcrun --sdk $(SDK) --find ar)
ifeq ($(CLANG),)
$(error Could not find clang for SDK $(SDK))
endif
SYSROOT := $(shell xcrun --sdk $(SDK) --show-sdk-path)
CC      := $(CLANG) -isysroot $(SYSROOT) -arch $(ARCH)
endif
CODESIGN := codesign

# Directories.

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LIB_DIR = lib
EXTERNAL_HDR_DIR = external

# Flags.

ERRFLAGS   = -Wall -Wpedantic -Wno-gnu -Werror
CFLAGS     = -g -O0 -I$(SRC_DIR) -I$(EXTERNAL_HDR_DIR) $(ERRFLAGS)
LDFLAGS    = -g
FRAMEWORKS = -framework Foundation -framework IOKit
ARFLAGS    = r

# libmemctl sources.

LIBMEMCTL_SRCS = core.c \
		 error.c \
		 kernel.c \
		 kernel_memory.c \
		 kernel_slide.c \
		 kernelcache.c \
		 macho.c \
		 memctl_common.c \
		 memctl_error.c \
		 memctl_offsets.c \
		 memctl_signal.c \
		 oskext.c \
		 platform.c

LIBMEMCTL_HDRS = core.h \
		 error.h \
		 kernel.h \
		 kernel_memory.h \
		 kernel_slide.h \
		 kernelcache.h \
		 macho.h \
		 memctl_common.h \
		 memctl_error.h \
		 memctl_offsets.h \
		 memctl_signal.h \
		 memctl_types.h \
		 offset.h \
		 oskext.h \
		 platform.h \
		 utility.h

LIBMEMCTL_OBJS := $(LIBMEMCTL_SRCS:%.c=$(OBJ_DIR)/%.o)

MEMCTL_LIB := $(LIB_DIR)/libmemctl.a

# memctl sources.

MEMCTL_DIR = cli

MEMCTL_SRCS = cli.c \
	      command.c \
	      error.c \
	      memctl.c \
	      read.c \
	      vmmap.c

MEMCTL_HDRS = cli.h \
	      command.h \
	      error.h \
	      format.h \
	      read.h \
	      vmmap.h

MEMCTL_SRCS := $(MEMCTL_SRCS:%=$(MEMCTL_DIR)/%)
MEMCTL_HDRS := $(MEMCTL_HDRS:%=$(MEMCTL_DIR)/%)
MEMCTL_OBJS := $(MEMCTL_SRCS:%.c=$(OBJ_DIR)/%.o)

MEMCTL_BIN := $(BIN_DIR)/memctl

# Targets.

.PHONY: all clean

vpath % $(SRC_DIR)

all: $(MEMCTL_BIN)

$(OBJ_DIR)/$(MEMCTL_DIR)/%.o: $(MEMCTL_DIR)/%.c $(MEMCTL_HDRS) $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(MEMCTL_BIN): $(MEMCTL_LIB) $(CORE_LIB) $(MEMCTL_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(FRAMEWORKS) $(MEMCTL_OBJS) $(CORE_LIB) -force_load $(MEMCTL_LIB) -o $@
	$(CODESIGN) -s - $@

$(MEMCTL_LIB): $(LIBMEMCTL_OBJS)
	@mkdir -p $(@D)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf -- $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)
