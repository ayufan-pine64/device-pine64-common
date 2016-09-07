# The MIT License (MIT)
# Copyright (c) 2016 Kamil Trzciński <ayufan@ayufan.eu>

# Permission is hereby granted, free of charge,
# to any person obtaining a copy of this software
# and associated documentation files (the "Software"),
# to deal in the Software without restriction,
# including without limitation the rights to
# use, copy, modify, merge, publish, distribute,
# sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice
# shall be included in all copies or substantial portions
# of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MULTILIB := both
LOCAL_MODULE := hdmi_cec.tulip
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    liblog \
    libdl

LOCAL_SRC_FILES += \
    sunxi_hdmi_cec.c

LOCAL_CFLAGS += -Wno-unused-parameter -D_ANDROID_

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := hdmi_cec.test
LOCAL_MODULE_TAGS := tests

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    liblog \
    libdl \
    libhardware

LOCAL_SRC_FILES += \
	sunxi_hdmi_cec.c \
	sunxi_hdmi_cec_test.c

LOCAL_CFLAGS += -Wno-unused-parameter -Wall

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := hdmi_cec.dump
LOCAL_MODULE_TAGS := tests

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    liblog \
    libdl \
    libhardware

LOCAL_SRC_FILES += \
	sunxi_hdmi_cec_dump.c

LOCAL_CFLAGS += -Wno-unused-parameter -Wall

include $(BUILD_EXECUTABLE)
