# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/lvgl $(IDF_PATH)/components/lvgl/src $(IDF_PATH)/components/lvgl/src/lv_core $(IDF_PATH)/components/lvgl/src/lv_draw $(IDF_PATH)/components/lvgl/src/lv_font $(IDF_PATH)/components/lvgl/src/lv_gpu $(IDF_PATH)/components/lvgl/src/lv_hal $(IDF_PATH)/components/lvgl/src/lv_misc $(IDF_PATH)/components/lvgl/src/lv_themes $(IDF_PATH)/components/lvgl/src/lv_widgets $(IDF_PATH)/components/lvgl/src/lv_lib_gif
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/lvgl -llvgl
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += lvgl
COMPONENT_LDFRAGMENTS += 
component-lvgl-build: 
