#
# Main Makefile. This is basically the same as a component makefile.
#
LOCAL_PATH := $(shell pwd) 
COMPONENT_SRCDIRS += ../lv_examples
COMPONENT_SRCDIRS += ../lv_examples/src/lv_demo_widgets/
 
# //这天用于添加，需要编译的头文件
 COMPONENT_ADD_INCLUDEDIRS += ../lv_examples
 COMPONENT_ADD_INCLUDEDIRS += ../lv_examples/src/lv_demo_widgets/