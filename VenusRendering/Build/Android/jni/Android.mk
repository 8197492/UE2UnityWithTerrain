LOCAL_PATH := $(call my-dir)/../../../Source

include $(CLEAR_VARS)

LOCAL_MODULE := VenusRendering

LOCAL_CFLAGS := -O2 -Wall -Werror

LOCAL_CXXFLAGS := $(LOCAL_CFLAGS) -std=c++1y -fno-exceptions -fno-rtti

LOCAL_C_INCLUDES := $(LOCAL_PATH)/Venus3D

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%.cpp,%.cpp,$(call rwildcard, $(LOCAL_PATH)/, *.cpp))

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)

BuildAll:
	ndk-build
	bash copy_to_plugin.sh
