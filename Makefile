#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
 LOCAL_PATH := $(shell pwd) 
PROJECT_NAME := gpio
COMPONENT_SRCDIRS += $(LOCAL_PATH)/lv_examples
COMPONENT_SRCDIRS += $(LOCAL_PATH)/lv_examples/src/lv_demo_widgets/
 
# //这天用于添加，需要编译的头文件
 COMPONENT_ADD_INCLUDEDIRS += $(LOCAL_PATH)/lv_examples
 COMPONENT_ADD_INCLUDEDIRS += $(LOCAL_PATH)/lv_examples/src/lv_demo_widgets/

include $(IDF_PATH)/make/project.mk

