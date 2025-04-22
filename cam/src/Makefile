SHELL = /bin/bash
ifeq ($(PARAM_FILE), )
	PARAM_FILE:=../../Makefile.param
	include $(PARAM_FILE)
endif
include ../sample.mk

SIGTEST_ENABLE = 0
H265_ENABLE = 1

SDIR = $(PWD)
SRCS = $(wildcard $(SDIR)/src/*.c) $(wildcard $(SDIR)/../vdec/src/*.c)
VENC_SRCS = $(SDIR)/recorder.c

ifeq ($(SIGTEST_ENABLE), 1)
SIGTEST_SRCS = $(SDIR)/sigtest.c
endif


VENC_INC=../../modules/venc
SAMPLE_VENC_INC=$(SDIR)/include
SAMPLE_VDEC_INC=$(SDIR)/../vdec/include
INCS = -I$(MW_INC) -I$(ISP_INC) -I$(COMM_INC) -I$(VENC_INC) -I$(KERNEL_INC) -I$(SAMPLE_VENC_INC) -I$(SAMPLE_VDEC_INC) -I../common -I$(SYS_INC)

OBJS = $(SRCS:.c=.o)
VENC_OBJS = $(VENC_SRCS:.c=.o)

ifeq ($(SIGTEST_ENABLE), 1)
SIGTEST_OBJS = $(SIGTEST_SRCS:.c=.o)
endif

DEPS = $(SRCS:.c=.d)
VENC_DEPS = $(VENC_SRCS:.c=.d)

ifeq ($(SIGTEST_ENABLE), 1)
SIGTEST_DEPS = $(SIGTEST_SRCS:.c=.d)
endif

TARGET_VENC = recorder

# Add H265-related targets
ifeq ($(H265_ENABLE), 1)
VENC_OBJS_H265 = $(SDIR)/recorder_h265.o  # Separate object for H265 target
DEFS_H265 = -DH265_ENABLE=1             # H265-specific macro definition
TARGET_VENC_H265 = recorder_h265       # New target name
endif

ifeq ($(SIGTEST_ENABLE), 1)
TARGET_SIGTEST = sigtest
else
TARGET_SIGTEST =
endif

ifeq ($(SDK_VER),32bit)
TARGET_VENC_WITH_ASAN = recorder_asan
else ifeq ($(SDK_VER), 64bit)
TARGET_VENC_WITH_ASAN = recorder_venc_asan
else
TARGET_VENC_WITH_ASAN =
endif

PKG_CONFIG_PATH = $(MW_PATH)/pkgconfig
REQUIRES = cvi_common cvi_sample cvi_vdec
MW_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --define-variable=mw_dir=$(MW_PATH) $(REQUIRES))

LIBS = $(MW_LIBS)
ifeq ($(MULTI_PROCESS_SUPPORT), 1)
DEFS += -DRPC_MULTI_PROCESS
LIBS += -lnanomsg
endif

ifeq ($(CLI_DEBUG_SUPPORT), 1)
DEFS += -DCLI_DEBUG_SUPPORT
LIBS += -lcli
endif

EXTRA_CFLAGS = $(INCS) $(DEFS)
EXTRA_LDFLAGS = $(LIBS) -lini -lpthread -latomic

EXTRA_LDFLAGS_WITH_ASAN = -Wl,-Bdynamic $(LIBS) -lini -lpthread

ifeq ($(DEBUG), 1)
EXTRA_LDFLAGS += -g -O0
EXTRA_LDFLAGS_WITH_ASAN += -g -O0
endif

EXTRA_LDFLAGS_WITH_ASAN += -fno-omit-frame-pointer -fsanitize-recover=address -lasan

.PHONY : clean all

all : $(TARGET_VENC) $(TARGET_VENC_H265) $(TARGET_SIGTEST) $(TARGET_VCODEC) $(TARGET_VENC_WITH_ASAN) $(TARGET_VCODEC_WITH_ASAN)

$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	@$(CC) $(DEPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
	@echo [$(notdir $(CC))] $(notdir $@)

$(SDIR)/%.o: $(SDIR)/%.c
	@$(CC) $(DEPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
	@echo [$(notdir $(CC))] $(notdir $@)

$(TARGET_VENC) : $(VENC_OBJS) $(OBJS) $(MW_LIB)/libvenc.a $(MW_LIB)/libvpu.a $(MW_LIB)/libsys.a
	@$(CXX) -o $@ $(OBJS) $(VENC_OBJS) $(ELFFLAGS) $(EXTRA_LDFLAGS)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CC))] $(notdir $@)


# Add build rules for H265 target
ifeq ($(H265_ENABLE), 1)
$(SDIR)/recorder_h265.o: $(SDIR)/recorder.c
	@$(CC) $(DEPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(DEFS_H265) -o $@ -c $<
	@echo [$(notdir $(CC))] $(notdir $@)

$(TARGET_VENC_H265) : $(VENC_OBJS_H265) $(OBJS) $(MW_LIB)/libvenc.a $(MW_LIB)/libvpu.a $(MW_LIB)/libsys.a
	@$(CXX) -o $@ $(OBJS) $(VENC_OBJS_H265) $(ELFFLAGS) $(EXTRA_LDFLAGS)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CC))] $(notdir $@)
endif
	
$(TARGET_SIGTEST) : $(SIGTEST_OBJS) $(OBJS)
	@$(CXX) -o $@ $(OBJS) $(SIGTEST_OBJS) $(ELFFLAGS) $(EXTRA_LDFLAGS)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CC))] $(notdir $@)

$(TARGET_VCODEC) : $(VCODEC_OBJS) $(OBJS) $(MW_LIB)/libvenc.a $(MW_LIB)/libvdec.a $(MW_LIB)/libvpu.a $(MW_LIB)/libsys.a
	@$(CXX) -o $@ $(OBJS) $(VCODEC_OBJS) $(ELFFLAGS) $(EXTRA_LDFLAGS)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CXX))] $(notdir $@)

$(TARGET_VENC_WITH_ASAN) : $(VENC_OBJS) $(OBJS)
	@$(CXX) -o $@ $(OBJS) $(VENC_OBJS) $(ELFFLAGS) $(EXTRA_LDFLAGS_WITH_ASAN)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CC))] $(notdir $@)

$(TARGET_VCODEC_WITH_ASAN) : $(VCODEC_OBJS) $(OBJS)
	@$(CXX) -o $@ $(OBJS) $(VCODEC_OBJS) $(ELFFLAGS) $(EXTRA_LDFLAGS_WITH_ASAN)
	@echo -e $(BLUE)[LINK]$(END)[$(notdir $(CXX))] $(notdir $@)

clean:
	@rm -f $(OBJS) $(VENC_OBJS) $(VENC_OBJS_H265) $(VCODEC_OBJS) $(DEPS) $(VENC_DEPS) $(VCODEC_DEPS) $(COMM_DEPS) $(COMM_OBJ) \
	$(TARGET_VENC) $(TARGET_VCODEC) $(TARGET_VENC_WITH_ASAN) $(TARGET_VCODEC_WITH_ASAN) $(TARGET_VENC_H265)
	
-include $(VENC_DEPS) $(VCODEC_DEPS) $(DEPS) $(COMM_DEPS)
