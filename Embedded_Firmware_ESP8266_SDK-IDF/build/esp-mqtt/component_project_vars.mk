# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/esp-mqtt/include $(IDF_PATH)/components/esp-http-parser-master/http-parser
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp-mqtt -lesp-mqtt
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp-mqtt
component-esp-mqtt-build: 
