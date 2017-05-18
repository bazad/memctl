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
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
LIB_DIR = lib
EXTERNAL_HDR_DIR = external

LIBMEMCTL_DIR     = libmemctl
LIBMEMCTL_INC_DIR = memctl
MEMCTL_DIR        = memctl

# Flags.

ERRFLAGS   = -Wall -Wpedantic -Wno-gnu -Werror
CFLAGS     = -g -O0 -I$(INC_DIR) $(ERRFLAGS)
LDFLAGS    = -g -lcompression
FRAMEWORKS = -framework Foundation -framework IOKit
ARFLAGS    = r

LIBMEMCTL_CFLAGS = -I$(SRC_DIR)/$(LIBMEMCTL_DIR) -I$(EXTERNAL_HDR_DIR)
MEMCTL_CFLAGS    = -I$(SRC_DIR)/$(MEMCTL_DIR) -I$(EXTERNAL_HDR_DIR)

ifeq ($(REPL),YES)
LDFLAGS        += -ledit -lcurses
MEMCTL_DEFINES += -DMEMCTL_REPL=1
endif

# libmemctl arm64/aarch64 sources.

ARCH_arm64_DIR = aarch64

LIBMEMCTL_arm64_SRCS = disasm.c \
		       kernel_call_aarch64.c \
		       memory_region.c

LIBMEMCTL_arm64_HDRS = kernel_call_aarch64.h

LIBMEMCTL_arm64_INCS = disasm.h

# libmemctl x86_64 sources.

ARCH_x86_64_DIR = x86_64

LIBMEMCTL_x86_64_SRCS = kernel_call_syscall_asm.s \
			kernel_call_syscall_x86_64.c \
			memory_region.c

LIBMEMCTL_x86_64_HDRS = kernel_call_syscall_code.h

LIBMEMCTL_x86_64_INCS = kernel_call_syscall_x86_64.h

# libmemctl sources.

LIBMEMCTL_ARCH_SRCS = $(LIBMEMCTL_$(ARCH)_SRCS:%=$(ARCH_$(ARCH)_DIR)/%)
LIBMEMCTL_ARCH_HDRS = $(LIBMEMCTL_$(ARCH)_HDRS:%=$(ARCH_$(ARCH)_DIR)/%)
LIBMEMCTL_ARCH_INCS = $(LIBMEMCTL_$(ARCH)_INCS:%=$(ARCH_$(ARCH)_DIR)/%)

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
		 memctl_common.h

LIBMEMCTL_INCS = $(LIBMEMCTL_ARCH_INCS) \
		 core.h \
		 error.h \
		 kernel.h \
		 kernel_call.h \
		 kernel_memory.h \
		 kernel_slide.h \
		 kernelcache.h \
		 macho.h \
		 memctl_error.h \
		 memctl_offsets.h \
		 memctl_signal.h \
		 memctl_types.h \
		 memory_region.h \
		 offset.h \
		 oskext.h \
		 platform.h \
		 utility.h \
		 vtable.h

LIBMEMCTL_SRCS := $(LIBMEMCTL_SRCS:%=$(SRC_DIR)/$(LIBMEMCTL_DIR)/%)
LIBMEMCTL_HDRS := $(LIBMEMCTL_HDRS:%=$(SRC_DIR)/$(LIBMEMCTL_DIR)/%)
LIBMEMCTL_INCS := $(LIBMEMCTL_INCS:%=$(INC_DIR)/$(LIBMEMCTL_INC_DIR)/%)
LIBMEMCTL_OBJS := $(LIBMEMCTL_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LIBMEMCTL_OBJS := $(LIBMEMCTL_OBJS:$(SRC_DIR)/%.s=$(OBJ_DIR)/%.o)

MEMCTL_LIB := $(LIB_DIR)/libmemctl.a

# memctl sources.

MEMCTL_arm64_SRCS = disassemble.c

MEMCTL_ARCH_SRCS = $(MEMCTL_$(ARCH)_SRCS:%=$(ARCH_$(ARCH)_DIR)/%)
MEMCTL_ARCH_HDRS = $(MEMCTL_$(ARCH)_HDRS:%=$(ARCH_$(ARCH)_DIR)/%)

MEMCTL_SRCS = $(MEMCTL_ARCH_SRCS) \
	      cli.c \
	      command.c \
	      error.c \
	      find.c \
	      format.c \
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

MEMCTL_SRCS := $(MEMCTL_SRCS:%=$(SRC_DIR)/$(MEMCTL_DIR)/%)
MEMCTL_HDRS := $(MEMCTL_HDRS:%=$(SRC_DIR)/$(MEMCTL_DIR)/%)
MEMCTL_OBJS := $(MEMCTL_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

MEMCTL_BIN := $(BIN_DIR)/memctl

# Targets.

.PHONY: all clean

all: $(MEMCTL_BIN)

ifneq ($(wildcard $(CORE_ENTITLEMENTS)),)
CODESIGN_FLAGS = --entitlements "$(CORE_ENTITLEMENTS)"
endif

$(OBJ_DIR)/$(MEMCTL_DIR)/%.o: $(SRC_DIR)/$(MEMCTL_DIR)/%.c $(MEMCTL_HDRS) $(LIBMEMCTL_INCS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MEMCTL_CFLAGS) $(MEMCTL_DEFINES) -c $< -o $@

$(OBJ_DIR)/$(LIBMEMCTL_DIR)/%.o: $(SRC_DIR)/$(LIBMEMCTL_DIR)/%.c $(LIBMEMCTL_INCS) $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LIBMEMCTL_CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(LIBMEMCTL_DIR)/%.o: $(SRC_DIR)/$(LIBMEMCTL_DIR)/%.s $(LIBMEMCTL_INCS) $(LIBMEMCTL_HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LIBMEMCTL_CFLAGS) -c $< -o $@

$(MEMCTL_BIN): $(MEMCTL_LIB) $(CORE_LIB) $(MEMCTL_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(FRAMEWORKS) $(MEMCTL_OBJS) "$(CORE_LIB)" -force_load $(MEMCTL_LIB) -o $@
	$(CODESIGN) $(CODESIGN_FLAGS) -s - $@

$(MEMCTL_LIB): $(LIBMEMCTL_OBJS)
	@mkdir -p $(@D)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf -- $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)
