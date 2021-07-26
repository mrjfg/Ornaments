
COMPONENT_SRCDIRS += driver/dev_imu/
COMPONENT_SRCDIRS += driver/dev_sdcard/
COMPONENT_SRCDIRS += driver/dev_spi/
COMPONENT_SRCDIRS += driver/qi/
 
# //这用于添加，需要编译的头文件
COMPONENT_ADD_INCLUDEDIRS += driver/dev_imu/include/
COMPONENT_ADD_INCLUDEDIRS += driver/dev_sdcard/include/
COMPONENT_ADD_INCLUDEDIRS += driver/dev_spi/include/
COMPONENT_ADD_INCLUDEDIRS += driver/qi/include/
COMPONENT_ADD_INCLUDEDIRS += ./