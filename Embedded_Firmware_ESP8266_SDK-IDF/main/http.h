#ifndef __HTTP_H__
#define __HTTP_H__

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include <netdb.h>
#include <sys/socket.h>

enum _config_error
{
    E_DNS_FAILED = -1,
    E_SOCKET_FAILED = -2,
    E_CONNECT_FAILED = -3, 
    E_SEND_FAILED = -4,
    E_SOCKET_TIMEOUT = -5,
    E_SOCKET_READ = -6,
    E_SEND_OK = 0,
};
typedef enum _config_error error_t;

#define HTTP_BUFFER_SIZE 1024 

struct http_conf
{
	 char * port;
	 const char * ip;
	 char * url; 
	 char 	http_buffer[HTTP_BUFFER_SIZE];
	 int 	http_buffer_size;
	 int 	http_buffer_qtd_bytes; 
};

typedef struct http_conf http_config_t;


signed int http_send( http_config_t * config );

#endif 