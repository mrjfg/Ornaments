#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
LOCAL_PATH := $(shell pwd) 
PROJECT_NAME := ornaments
# COMPONENT_SRCDIRS += $(LOCAL_PATH)/dev_imu/
# COMPONENT_SRCDIRS += $(LOCAL_PATH)/dev_sdcard/
# COMPONENT_SRCDIRS += $(LOCAL_PATH)/dev_spi/

 
# //这用于添加，需要编译的头文件
# COMPONENT_ADD_INCLUDEDIRS += $(LOCAL_PATH)/dev_imu/include/
# COMPONENT_ADD_INCLUDEDIRS += $(LOCAL_PATH)/dev_sdcard/include/
# COMPONENT_ADD_INCLUDEDIRS += $(LOCAL_PATH)/dev_spi/include/


include $(IDF_PATH)/make/project.mk

