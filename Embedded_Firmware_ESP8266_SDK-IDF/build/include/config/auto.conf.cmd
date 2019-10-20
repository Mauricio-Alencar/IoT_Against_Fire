deps_config := \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/app_update/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/aws_iot/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/esp-mqtt/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/esp8266/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/freertos/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/libsodium/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/log/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/lwip/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/mdns/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/newlib/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/pthread/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/ssl/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/tcpip_adapter/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/wpa_supplicant/Kconfig \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/bootloader/Kconfig.projbuild \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/esptool_py/Kconfig.projbuild \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/components/partition_table/Kconfig.projbuild \
	/c/ESP8266/ESP8266_RTOS_SDK-3.1/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
