# swresample

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../av.mk

LOCAL_SRC_FILES := $(FFFILES)


# NOTE: important to have ../../jni first to look for overridden headers, such as ffversion.h
LOCAL_C_INCLUDES :=        \
    $(libsoxr_PATH) \
    $(libsoxr_PATH)/soxr-0.1.3/src \
    $(LOCAL_PATH)        \
    $(LOCAL_PATH)/..    \
    $(FFMPEG_LOCAL_PATH)        \
    $(FFMPEG_LOCAL_PATH)/.. \

LOCAL_CFLAGS := $(PA_GLOBAL_CFLAGS)

LOCAL_CFLAGS += -fno-stack-protector

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    # Best for ndk-23
    # Reenabled for ndk-29 where it's 2x faster with neon
    #LOCAL_CFLAGS += -DPAMP_DISABLE_NEON_ASM
    # Oz seems to be as fast as Os
    LOCAL_CFLAGS += -O3
    LOCAL_CFLAGS += -Wno-logical-op-parentheses -Wno-switch
else ifneq (,$(findstring armeabi-v7a,$(TARGET_ARCH_ABI)))
    LOCAL_CFLAGS += -O3
endif

LOCAL_CFLAGS += $(PA_GLOBAL_OVERRIDE_CFLAGS)
#$(error $(LOCAL_CFLAGS))

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := $(FFLIBS)

LOCAL_MODULE := $(FFNAME)

ifeq (,$(findstring -O, $(LOCAL_CFLAGS))) # Check for optimization flag
    $(error No -O in LOCAL_CFLAGS=$(LOCAL_CFLAGS))
endif
ifneq (false,$(PA_GLOBAL_FLTO)) # NOTE: PA_GLOBAL_FLTO can be false,full,thin
    ifeq (,$(findstring -flto, $(LOCAL_CFLAGS)))
        $(error No -flto in LOCAL_CFLAGS=$(LOCAL_CFLAGS))
    endif
endif


include $(BUILD_STATIC_LIBRARY)
