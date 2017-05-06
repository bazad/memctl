# Makefile

# Compilation options.

ARCH     ?= arm64
SDK      ?= iphoneos
CORE_DIR ?= core
CORE_LIB ?= $(CORE_DIR)/libmemctl_core.a
CORE_ENTITLEMENTS ?= $(CORE_DIR)/entitlements.plist

REPL     ?= YES

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
LDFLAGS    = -g -lcompression
FRAMEWORKS = -framework Foundation -framework IOKit
ARFLAGS    = r

ifeq ($(REPL),YES)
LDFLAGS        += -ledit -lcurses
MEMCTL_DEFINES += -DMEMCTL_REPL=1
endif

# libmemctl aarch64 sources.

LIBMEMCTL_AARCH64_SRCS = aarch64/disasm.c \
			 aarch64/kernel_call_aarch64.c

LIBMEMCTL_AARCH64_HDRS = aarch64/disasm.h \
			 aarch64/kernel_call_aarch64.h

# libmemctl sources.

ifeq ($(ARCH),arm64)
LIBMEMCTL_ARCH_SRCS = $(LIBMEMCTL_AARCH64_SRCS)
LIBMEMCTL_ARCH_HDRS = $(LIBMEMCTL_AARCH64_HDRS)
endif

LIBMEMCTL_SRCS = $(LIBMEMCTL_ARCH_SRCS) \
		 core.c \
		 error.c \
		 kernel.c \
		 kernel_call.c \
		 kernel_memory.c \
		 kernel_slide.c \
		 kernelcache.c \
		 macho.c \
		 memctl_common.c \
		 memctl_error.c \
		 memctl_offsets.c \
		 memctl_signal.c \
		 oskext.c \
		 platform.c \
		 vtable.c

LIBMEMCTL_HDRS = $(LIBMEMCTL_ARCH_HDRS) \
		 core.h \
		 error.h \
		 kernel.h \
		 kernel_call.h \
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
		 utility.h \
		 vtable.h

LIBMEMCTL_OBJS := $(LIBMEMCTL_SRCS:%.c=$(OBJ_DIR)/%.o)

MEMCTL_LIB := $(LIB_DIR)/libmemctl.a

# memctl sources.

MEMCTL_DIR = cli

MEMCTL_AARCH64_SRCS = aarch64/disassemble.c

ifeq ($(ARCH),arm64)
MEMCTL_ARCH_SRCS = $(MEMCTL_AARCH64_SRCS)
endif

MEMCTL_SRCS = $(MEMCTL_ARCH_SRCS) \
	      cli.c \
	      command.c \
	      error.c \
	      find.c \
	      memctl.c \
	      memory.c \
	      read.c \
	      vmmap.c

MEMCTL_HDRS = $(MEMCTL_ARCH_HDRS) \
	      cli.h \
	      command.h \
	      disassemble.h \
	      error.h \
	      find.h \
	      format.h \
	      memory.h \
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

ifneq ($(wildcard $(CORE_ENTITLEMENTS)),)
CODESIGN_FLAGS = --entitlements "$(CORE_ENTITLEMENTS)"
endif

$(OBJ_DIR)/$(MEMCTL_DIR)/%.o: $(MEMCTL_DIR)/%.c $(MEMCTL_HDRS) $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MEMCTL_DEFINES) -c $< -o $@

$(OBJ_DIR)/%.o: %.c $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(MEMCTL_BIN): $(MEMCTL_LIB) $(CORE_LIB) $(MEMCTL_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(FRAMEWORKS) $(MEMCTL_OBJS) "$(CORE_LIB)" -force_load $(MEMCTL_LIB) -o $@
	$(CODESIGN) $(CODESIGN_FLAGS) -s - $@

$(MEMCTL_LIB): $(LIBMEMCTL_OBJS)
	@mkdir -p $(@D)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf -- $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)
