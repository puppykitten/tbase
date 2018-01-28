LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	tee.cpp \

LOCAL_SHARED_LIBRARIES += libutils libbinder libcutils libgui liblog
LOCAL_LDLIBS += -lmediandk -lutils -lbinder -llog

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_CFLAGS += -O0
CFLAGS += -S

LOCAL_MULTILIB := 64

LOCAL_MODULE:= tee

include $(BUILD_EXECUTABLE)
