/*
PROJETO - IoT against Fire NASA

Toolchain: ESP8266 RTOS-SDK (SDK-IDF)
RTOS: FreeRTOS
Data_inicial: 19/08/2019

Desenvolvido por Maurício Alencar.
*/

/**
 * C library
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/**
 * FreeRTOS
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/**
 * Drivers
 */
#include "driver/gpio.h"

/**
 * Driver nvs_flash
 */
#include "nvs_flash.h"

/**
 * Drivers TCP
 */
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <lwip/sockets.h>
#include "lwip/api.h"
#include "lwip/netdb.h"
#include <sys/socket.h>
#include <netdb.h>

/**
 * HTTP
 */
#include "http.h"

/*
 * MQTT
 */
#include "mqtt_client.h"

/**
 * cJSON
 */
#include "cJSON.h"

/**
 * Logs
 */
#include "esp_log.h"
#include "esp_system.h"
#include <errno.h>

/**
 * Definições Gerais
 */
#define TRUE  1
#define FALSE 0
#define DEBUG TRUE 
static const char *TAG = "main";

#define TOPICO_COUNTER  "test/counter"
#define TOPICO_LED      "test/led"
#define TOPICO_ALARME   "alarme"

#define WIFI_CONNECTED          0
#define WIFI_CONFIG             1
#define WIFI_DISCONNECTED       2

/*
 * Configuração do WiFiManager, posteriormente transformar em biblioteca
 */
#define SSID_WIFI "IoTagainstFire"
#define PWD_WIFI  "11111111"
#define SSID_PASSWORD_SIZE 	30

extern const uint8_t server_html_page_form_start[] asm("_binary_server_html_page_form_start");
extern const uint8_t server_html_page_form_end[]   asm("_binary_server_html_page_form_end");

const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";

/**
 * Definições das GPIOs
 */
#define LED_BUILDING         2
#define GPIO_OUTPUT_LED_PIN_SEL  ( (1ULL<<LED_BUILDING) )

#define BUTTON               0
#define GPIO_INPUT_BUTTON_PIN_SEL  ( (1ULL<<BUTTON) )

#define PULSE                5
#define GPIO_INPUT_PULSE_PIN_SEL  ( (1ULL<<PULSE) )


/**
 * Declarações Globais
 */
static EventGroupHandle_t wifi_event_group_sta;
static EventGroupHandle_t wifi_event_group_ap;
static EventGroupHandle_t mqtt_event_group;
const static int CONNECTED_BIT = BIT0;
xQueueHandle gpio_evt_queue = NULL;
xQueueHandle alarm_evt_queue = NULL;
xQueueHandle status_wifi_blink = NULL;
static esp_mqtt_client_handle_t client;
char wifi_ssid[SSID_PASSWORD_SIZE];
char wifi_password[SSID_PASSWORD_SIZE];
uint8_t queue_sta = WIFI_DISCONNECTED;
static uint8_t bootloader_AP = FALSE;

/**
 * Prototipos de funções
 */
void app_main(void);
void init_gpio(void);
void wifi_config_sta(char* ssid, char* password);
void init_wifi_sta(void);
void init_mqtt(void);
esp_err_t wifi_event_handler(void *ctx, system_event_t *event);
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
void task_button(void *pvParameters);
void task_check_alarm(void *pvParameters);
void task_blink(void * pvParameters);
void task_button_config_http_server(void *pvParameters);
void task_monitoring(void *pvParameter);
void task_http_server_wifimanger(void *pvParameters);
void create_queue(void);
void create_task(void);
signed int processJson_MQTT(char* str, uint32_t* result);
void http_server_netconn_serve(struct netconn *conn);
void start_dhcp_server( void );
void init_wifi_ap(void);
esp_err_t nvs_str_save(char * key, char * value);
esp_err_t nvs_read_ssid_password(char * ssid, size_t ssid_len, char* password, size_t password_len);

//
/**************************************************** INICIO DE PROGRAMA *****************************************************************/
void app_main( void )
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    /**
     * Inicialização da NVS. 
     * Obrigatória a chamada para calibração do PHY do ESP8266.
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Criação de filas necessarias no Kernel
    create_queue();
    init_gpio();

    //criação de tarefa que gerencia status_led para wifi
    if(xTaskCreate( task_blink, "task_blink", 2048, NULL, 2, NULL) != pdTRUE )
    {
        if( DEBUG )
            ESP_LOGI(TAG, " task_blink error " );
        while( TRUE );
    }

    /*
    * verificação de bootloader em modo_station ou config_ap resgatando parâmetro da NVS
    */
    nvs_handle my_handle;
    esp_err_t err = nvs_open( "storage", NVS_READWRITE, &my_handle );
    if ( err != ESP_OK ) {					
            if( DEBUG )
                ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err );			
    } else {
        err = nvs_get_u8(my_handle, "Boot_AP", &bootloader_AP);
        if(err != ESP_OK) {
            if( err == ESP_ERR_NVS_NOT_FOUND ) {
				if( DEBUG )
					ESP_LOGI(TAG, "\nKey Boot_AP not found\n" );
				bootloader_AP = FALSE;
			} 										
        }    
        ESP_LOGI(TAG, "Valor Bootlaoder_AP: %d", bootloader_AP);                                    
        nvs_close(my_handle);		
    }

    if(bootloader_AP)
    {   
        init_wifi_ap();

        //criação de handle para nvs
        nvs_handle my_handle;
	    esp_err_t err = nvs_open( "storage", NVS_READWRITE, &my_handle );
        if ( err != ESP_OK ) {					
			if( DEBUG )
				ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err );			
	    } else {
            err = nvs_set_u8(my_handle, "Boot_AP", FALSE);
                if(err != ESP_OK) {
                    if( DEBUG )
                        ESP_LOGI(TAG, "\nError in nvs_set_u8 Boot_AP: (%04X)\n", err);		
                }
            err = nvs_commit(my_handle);
                if(err != ESP_OK) {
                    if( DEBUG )
                        printf("\nError in commit! (%04X)\n", err);
                }
                else{
                    ESP_LOGI(TAG, "Commit para NVS em modo Boot_AP Realizado!");
                } 
            nvs_close(my_handle);
            }

        //criação de tarefa para abertura de servidor AP.
        if( xTaskCreate( task_http_server_wifimanger, "task_http_server_wifimanger", 10048, NULL, 5, NULL ) != pdTRUE )
        {
            if( DEBUG )
                ESP_LOGI(TAG, " http_server error " );
            while( TRUE );
        }
    }
    else{
        init_wifi_sta();
        init_mqtt();
        //criação de task sempre deverá ser o ultimo passo da inicialização de bootloader, pois, no momento em que as task são criadas, o Kernel começa a fazer a troca de contexto.
        create_task();
    }
    
}
/******************************************************* FIM DE PROGRAMA ******************************************************************/


/*********************************************************** TAREFAS **********************************************************************/

void task_check_alarm(void *pvParameters){
    char res[30]; 
    uint32_t counter = 0;

    for (;;) 
    {
        //Event Group que aguarda conexão MQTT
        xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

        if(!gpio_get_level(PULSE))
        {
            /*Delay para tratamento do bounce da tecla*/
            vTaskDelay(80 / portTICK_PERIOD_MS);

            /**
             * Confirma se realmente o botão está pressionado, ou seja, se realmente
             * foi o usuário que pressionou o botão e não foi nenhum acionamento falso.
             */
            if(!gpio_get_level(PULSE))   
            {
                if( DEBUG )
                    ESP_LOGI(TAG, "Alarme Acionado!"); 

                /**
                 * Como a variável counter está sendo incrementada depois de ser enviada na fila, seu valor é iniciado em 1.
                 */
                sprintf(res, "{\"Evento\": \"Um principio de incendio foi detectado. Acalme-se e siga as insrtruções de segurança que o bombeiro foi acionado.\", \"value\":%d}", counter);

                /**
                    * int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);
                    * esp_mqtt_client_handle_t client: Handle da conexão, 
                    * const char *topic: Tópico, 
                    * const char *data: mensagem em string a ser enviada
                    *  int len: qtd de bytes da string
                    *  int qos: qos da mensagem 0, 1 ou 2,
                    *  int retain: reter a mensagem no broker-> 1, não -> 0
                    */
                if( esp_mqtt_client_publish(client, TOPICO_ALARME, res, strlen(res), 0, 0 ) == 0) {    
                    if( DEBUG )
                        ESP_LOGI( TAG, "Json %s foi Publicado via MQTT.\n", res );
                    counter++;
                }  else {
                    if( DEBUG )
                        ESP_LOGI( TAG, "Nao foi possivel publicar via MQTT.\n");                
                }  
              
            }
        }
        /**
         * É muito importante que em algum momento a task seja bloqueada para que o scheduler
         * possa devolver o controle para uma task de menor prioridade, tal como a task idle.
         */
        vTaskDelay( 500 / portTICK_RATE_MS );
    }

    vTaskDelete(NULL);
}

void task_button(void *pvParameters){
    char res[30]; 
    int aux=0;
    uint32_t counter = 0; 

    for (;;) 
    {
        //xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);


        if( !gpio_get_level( BUTTON ) && aux == 0 )
        {
            /* Delay para tratamento do bounce da tecla*/
            vTaskDelay( 80 / portTICK_PERIOD_MS );

            /**
             * Confirma se realmente o botão está pressionado, ou seja, se realmente
             * foi o usuário que pressionou o botão e não foi nenhum acionamento falso.
             */
            if( !gpio_get_level( BUTTON ) && aux == 0 )
            {
                if( DEBUG )
                    ESP_LOGI(TAG, " Button Pressionado."); 

                /**
                 * Como a variável counter está sendo incrementada depois de ser enviada na fila, seu valor é iniciado em 1.
                 */
                counter++;
                sprintf(res, "{\"value\":%d}", counter);

                /**
                 *int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);
                    * esp_mqtt_client_handle_t client: Handle da conexão, 
                    * const char *topic: Tópico, 
                    * const char *data: mensagem em string a ser enviada
                    *  int len: qtd de bytes da string
                    *  int qos: qos da mensagem 0, 1 ou 2,
                    *  int retain: reter a mensagem no broker-> 1, não -> 0
                    */
                if( esp_mqtt_client_publish( client, TOPICO_COUNTER, res, strlen(res), 0, 0 ) == 0) {    
                    if( DEBUG )
                        ESP_LOGI( TAG, "Json %s foi Publicado via MQTT.\n", res );
                    counter++;
                }  else {
                    if( DEBUG )
                        ESP_LOGI( TAG, "Nao foi possivel publicar via MQTT.\n");                
                }  
                
                
                aux = 1; 
            }
        }

        if( gpio_get_level( BUTTON ) && aux == 1 )
        {
            vTaskDelay( 50 / portTICK_RATE_MS ); 

            if( gpio_get_level( BUTTON ) && aux == 1 )
            {
                if( DEBUG )
                    ESP_LOGI(TAG, " Button Solto." );

                if( xQueueSend( gpio_evt_queue, (void*) &counter, (TickType_t) 10) == pdPASS )
                {
                    if( DEBUG )
                        ESP_LOGI(TAG, " Counter = %d. Enviado na fila.", counter );   
                }

                aux = 0;              
            }
        }

        /**
         * É muito importante que em algum momento a task seja bloqueada para que o scheduler
         * possa devolver o controle para uma task de menor prioridade, tal como a task idle.
         */
        vTaskDelay( 100 / portTICK_RATE_MS );

    }

    vTaskDelete(NULL);
}

void task_button_config_http_server(void *pvParameters)
{
    uint8_t event_reset = 1;
    for(;;)
    {
        xQueueReceive(gpio_evt_queue, &event_reset, portMAX_DELAY);
        if(DEBUG) ESP_LOGI(TAG, "Resetando para configuração em modo AP\n");

        //criação de handle para nvs
        nvs_handle my_handle;
	    esp_err_t err = nvs_open( "storage", NVS_READWRITE, &my_handle );
        if ( err != ESP_OK ) {					
			if( DEBUG )
				ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err );			
	    } else {
        err = nvs_set_u8(my_handle, "Boot_AP", event_reset);
            if(err != ESP_OK) {
                if( DEBUG )
                    ESP_LOGI(TAG, "\nError in nvs_set_u8 Boot_AP: (%04X)\n", err);		
            }
        err = nvs_commit(my_handle);
            if(err != ESP_OK) {
                if( DEBUG )
                    printf("\nError in commit! (%04X)\n", err);
            }
            else ESP_LOGI(TAG, "Commit para NVS em modo Boot_AP Realizado!");
        nvs_close(my_handle);
        }
        esp_restart();
    }
}

void task_blink( void * pvParameters )
{
    if( DEBUG )
        ESP_LOGI(TAG, "task blink ...." );
    uint8_t status_wifi = WIFI_CONNECTED;

    for (;;) 
    {
        xQueueReceive(status_wifi_blink, &status_wifi, pdMS_TO_TICKS(1000));

        if(status_wifi == WIFI_CONNECTED) gpio_set_level(LED_BUILDING, 1);
        if(status_wifi == WIFI_CONFIG) gpio_set_level(LED_BUILDING, 0);
        if(status_wifi == WIFI_DISCONNECTED) gpio_set_level(LED_BUILDING, !gpio_get_level(LED_BUILDING));        
    }
}

/*
 * Task Responsável pela configuração do servidor Web porta 80;
 */
void task_http_server_wifimanger( void *pvParameters ) 
{
	struct netconn *conn, *newconn;
	err_t err;
    uint8_t counter = 0;
	for(;;)
	{
		/**
		 * Associa o endereço IP do ESP8266 com a porta 80;
		 */
		conn = netconn_new( NETCONN_TCP );
		netconn_bind( conn, NULL, 80 );  //porta 80
		
		/**
		 * A partir de agora o servidor ficará aguardando
		 * a conexão socket de algum cliente;
		 */
		netconn_listen(conn);
		
		if( DEBUG )
			ESP_LOGI(TAG, "HTTP Server listening...\n" );	
		
		do {

			/**
			 * A função netconn_accept é bloqueante, portanto, 
			 * o programa ficará aguardando a conexão de um cliente nesta 
			 * linha; Quando um cliente se conectar o handle da conexão "newconn"
			 * será passada para a função http_server_netconn_serve, para que 
			 * seja feita a leitura e envio dos cabeçalhos HTTP e HTML.
			 * 
			 * Atenção: Cada request no HTML, GET, POST, será criado um novo socket;
			 */
			err = netconn_accept(conn, &newconn);
			if( DEBUG )
				ESP_LOGI(TAG, "Novo dispositivo conectado: Handler: %p\n", newconn );

			/**
			 * Caso a conexão do cliente tenha ocorrido com sucesso, 
			 * chama a função http_server_netconn_serve;
			 */
			if (err == ERR_OK) {

				/**
				 * Esta função é responsável em enviar a primeira pagina HTML
				 * para o cliente e de receber as requisições socket do client.
				 */
				http_server_netconn_serve(newconn);
				netconn_delete(newconn);
			}

			/**
			 * Em algum momento a task deverá ser bloqueada para dar
			 * chance das demais serem executadas.  Portanto, neste caso 
			 * bloqueia esta task por 1 segundo;
			 */
			vTaskDelay(10); 
		} while(err == ERR_OK);

		/**
		 * Caso ocorra algum erro durante a conexão socket com o cliente, 
		 * encerra o socket e deleta o handle da conexão;
		 */
		netconn_close(conn);
		netconn_delete(conn);
		
		if( DEBUG )
			ESP_LOGI(TAG, "Conexão Socket Encerrada: Handle:  %p\n", newconn );

    }

    /**
     * Em teoria, esta linha nunca será executada...
     */
	vTaskDelete(NULL); 
}

void task_monitoring(void *pvParameter)
{
    for(;;)
    {
        if( DEBUG )
            ESP_LOGI( TAG,"free heap: %d\n",esp_get_free_heap_size());
        vTaskDelay( 5000 / portTICK_PERIOD_MS );
    }
}

/********************************************************* FIM DE TAREFAS **********************************************************************/

/********************************************************* EVENTS HANDLER **********************************************************************/

/**
 * EVENT HANDLER para conexão com WiFi no modo Station.
 */
esp_err_t wifi_event_handler( void *ctx, system_event_t *event )
{
    switch( event->event_id ) 
    {
        /**
         * Chamado inicialmente quando o stack tcp for inicializado. 
         * Inicia a tentativa de conexão com a rede WiFi. 
         */
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            ESP_LOGI( TAG, "ESP8266 Inicializado em modo STATION e tentando conexão.\n" );
            queue_sta = 2;
            xQueueSend(status_wifi_blink, (void*) &queue_sta, pdMS_TO_TICKS(10));
            break;
        
        /**
         * Se o ESP8266 recebeu o IP, significa que a conexão entre ESP e Roteador 
         * correu com sucesso! 
         * Sinaliza por meio dos event groups do FreeRTOs que a conexão WiFi foi estabelecida e envia para fila resultado para led_building.
         * Portanto, já é possivel iniciar as tarefas que fazem o uso da rede WiFi.
         */
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa( &event->event_info.got_ip.ip_info.ip ));
            xEventGroupSetBits( wifi_event_group_sta, CONNECTED_BIT );

            queue_sta = 0;
            xQueueSend(status_wifi_blink, (void*) &queue_sta, pdMS_TO_TICKS(10));
            break;

        /**
         * Se por algum motivo a conexão com a rede WiFi cair, tenta novamente se reconectar.
         * Neste caso, reseta o event group responsável em sinalizar o status da conexão WiFi, 
         * pois as demais tarefas precisam saber que a conexão WiFi está offline.
         */
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits( wifi_event_group_sta, CONNECTED_BIT );

            queue_sta = WIFI_DISCONNECTED;
            xQueueSend(status_wifi_blink, (void*) &queue_sta, pdMS_TO_TICKS(10));
            break;
        
        case SYSTEM_EVENT_AP_START:
			if ( DEBUG )
				ESP_LOGI( TAG, "ESP8266 Inicializado em modo Acess Point.\n" );

                uint8_t queue_ap = WIFI_CONFIG;
                xQueueSend(status_wifi_blink, (void*) &queue_ap, pdMS_TO_TICKS(10));
			break;
        
        case SYSTEM_EVENT_AP_STACONNECTED:
			if( DEBUG )
				ESP_LOGI( TAG, "WiFi AP Conectado.\n" );
			
			/**
			 * Se chegou aqui, é porque o WiFi foi inicializado corretamente
			 * em modo Acess Point; Sinaliza por meio do eventgroup que 
			 * dispositivo está conectado;
			 */
			xEventGroupSetBits( wifi_event_group_ap, CONNECTED_BIT );			
			break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:		
			if( DEBUG )
				ESP_LOGI(TAG, "WiFi AP Desconectado\n");		
			
			/*Informa que um cliente foi desconectado do AP */
			xEventGroupClearBits(wifi_event_group_ap, CONNECTED_BIT);
			break;

        default:
            break;
    }

    return ESP_OK;
}

/**
 * EVENT HANDLER para conexão com MQTT.
 */
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    int msg_id;
    client = event->client;
    
    switch (event->event_id) {
        
        case MQTT_EVENT_CONNECTED:
            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            
            /**
             * Assina o tópico LED. As mensagens são recebida em case MQTT_EVENT_DATA.
             */
            msg_id = esp_mqtt_client_subscribe(client, TOPICO_LED, 0);

            /**
             * Sinaliza que o ESP está conectado ao broker MQTT.
             */
            xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);

            break;

        case MQTT_EVENT_DISCONNECTED:
            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");   
            xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT);

            break;

        case MQTT_EVENT_SUBSCRIBED:
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            break;
            
        case MQTT_EVENT_PUBLISHED:
            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_DATA");

            /**
             * Aloca Tópico;
             */
            char* topico = NULL;
            topico = (char*)pvPortMalloc(event->topic_len+1);
            if( topico == NULL ) {
                if ( DEBUG )
                    ESP_LOGI( TAG, "Error: malloc\r\n" ); 
                break;
            }
            sprintf( topico, "%.*s", event->topic_len, event->topic );

            /**
             * Verifica se quem publicou o tópico foi realmente TOPICO_COUNTER.
             */
            if(strcmp(topico, TOPICO_LED) == 0)
            {
                char* json = NULL;
                json = (char*)pvPortMalloc(event->data_len+1);
                if(json == NULL) {
                    if ( DEBUG )
                        ESP_LOGI( TAG, "Error: malloc\r\n" ); 
                    
                    free(topico);
                    break;
                }

                sprintf(json, "%.*s", event->data_len, event->data);
                /**
                 * {"results":[{"name":"Temperatura","value":1,"timestamp":1552674328,
                 * "description":"","unit":"\u00b0C"}]}
                 */
                if ( DEBUG )
                    ESP_LOGI( TAG, "%s\r\n", json ); 

                /**
                 * Processa a mensagem JSON recebida.
                 */
                uint32_t result;

                if( processJson_MQTT( json, &result) == 0)
                {

                    if( xQueueSend( gpio_evt_queue, ( void * ) &result, ( TickType_t ) 10 ) != pdPASS )
                    {
                        /* Failed to post the message, even after 10 ticks. */
                    }

                } else {

                    if ( DEBUG )
                        ESP_LOGI(TAG, "cJSON: Key \"value\" nao localizada." );  
                }

                free(json);

            } else {
                    if ( DEBUG )
                        ESP_LOGI(TAG, "Error." );                 
            }
            free(topico);

            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

/***************************************************** FIM DE EVENTS HANDLER **************************************************************/


/*********************************************************** FUNÇÕES **********************************************************************/

void init_gpio(void){
    //instancia do descritor (objeto) para configuração das GPIO
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_LED_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_level(LED_BUILDING, 1);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_BUTTON_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PULSE_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
}

void wifi_config_sta(char* ssid, char* password)
{
    tcpip_adapter_init();

    /**
     * Informa qual é a função de callback do WiFi. Ou seja, 
     * qual é a função que irá receber os retornos dos status da conexão WiFi.
     */
    ESP_ERROR_CHECK(esp_event_loop_init( wifi_event_handler, NULL ) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init( &cfg ));

    /**
     * A configuração da rede WiFi é carregada como constante no programa para somente ser inicializado o descritor 
     */
    wifi_config_t wifi_config = { 
        .sta = {
            .ssid = "",
            .password = ""
        },
    };
    
    //é feito uma copia do parametro passado na função com o descritor wifi_config
   memcpy(wifi_config.sta.ssid, ssid, (strlen(ssid) + 1));
   memcpy(wifi_config.sta.password, password, (strlen(password) + 1));

    //é inicializado o wifi com os parametros de descritores passados com sucesso.
    ESP_ERROR_CHECK(esp_wifi_set_mode( WIFI_MODE_STA ) );
    ESP_ERROR_CHECK(esp_wifi_set_config( ESP_IF_WIFI_STA, &wifi_config ) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    if( DEBUG ) {
        ESP_LOGI( TAG, "wifi_init_sta finished." );
        ESP_LOGI( TAG, "connect to ap SSID:%s password:%s",  (const char*)wifi_config.sta.ssid,  (const char*)wifi_config.sta.password );
    }
}


void init_wifi_sta(void){    
    //Criação de event_group para wifi_sta
    wifi_event_group_sta = xEventGroupCreate();
   
    /**
	 * Responsável em resgatar da nvs o valor do 
	 * ssid e password que foram salvos durante pelo usuário
	 * na página html (formulário);
	 */
	if( nvs_read_ssid_password( wifi_ssid, sizeof(wifi_ssid), wifi_password, sizeof(wifi_password) ) == ESP_OK )
	{
        if( DEBUG ) {
            ESP_LOGI(TAG, "ssid: %s", wifi_ssid );
            ESP_LOGI(TAG, "password: %s", wifi_password );	
        }	
	}

    /**
     * Inicializa o WiFi no modo Station passando a senha salva em memoria flash
     */
    if( DEBUG )
        ESP_LOGI( TAG, "ESP_WIFI_MODE_STATION" );
    wifi_config_sta(wifi_ssid,wifi_password);

    /**
     * Aguarda a conexão do ESP8266 na rede WiFi local.
     */
    xEventGroupWaitBits( wifi_event_group_sta, CONNECTED_BIT, false, true, pdMS_TO_TICKS(5000) );

     if( DEBUG )
        ESP_LOGI( TAG, "Dispositivo Conectado a Rede WiFi!" );
}

void create_queue(void){
    
    //criação de fila para troca de dados entre acionamentos de GPIO
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if(gpio_evt_queue == NULL){
        if(DEBUG) ESP_LOGI(TAG, " error create gpio queue ...");

        while(TRUE);
    }

    //criação de fila para troca de dados entre status do wifi
    status_wifi_blink = xQueueCreate(10, sizeof(uint32_t));
    if(status_wifi_blink == NULL)
    {
        if(DEBUG) ESP_LOGI(TAG, "error create wifi blink queue ...");
        while(TRUE);
    }
}

void create_task(void){
    
    if(xTaskCreate( task_button, "task button ", 2048, NULL, 2, NULL) != pdTRUE){
        if(DEBUG) ESP_LOGI(TAG, "task_button create error ...");
        while(TRUE);
    }

    if(xTaskCreate( task_check_alarm, "task alarme ", 2048, NULL, 2, NULL) != pdTRUE){
        if(DEBUG) ESP_LOGI(TAG, "task_button create error ...");
        while(TRUE);
    }

    if(xTaskCreate( task_button_config_http_server, "task button config http server", 2048, NULL, 2, NULL) != pdTRUE){
        if(DEBUG) ESP_LOGI(TAG, "task_button create error ...");
        while(TRUE);
    }

    if(xTaskCreate(task_monitoring, "task_monitoring", 2048, NULL, 2, NULL) != pdTRUE )
    {
        if( DEBUG )
            ESP_LOGI(TAG, " monitoring_task error " );
        while( TRUE );
    }
}

void init_mqtt(void)
{
    mqtt_event_group = xEventGroupCreate();

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://54.149.157.218:1883",
        .event_handle = mqtt_event_handler,
        .username = "admin",
		.password = "123",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

/**
 * Esta função é responsável em ler o JSON e retornar o valor do compo "value";
 * mensagem Json:
 * {"results":[{"name":"Temperatura","value":1,"timestamp":1552674328,"description":"","unit":"\u00b0C"}]}
 */
int32_t processJson_MQTT(char* str, uint32_t* result )
{
    int32_t error = -1; 

    cJSON *resolution = NULL;
    cJSON *resolutions = NULL;
    cJSON *json = cJSON_Parse(str);

    if(json != NULL)
    {

        resolutions = cJSON_GetObjectItem(json, "results");   
        if(resolutions != NULL)
        {
                cJSON_ArrayForEach(resolution, resolutions)
                {
                   cJSON *shortcut = cJSON_GetObjectItem((cJSON*)resolution, "value");
                   
                    if(cJSON_IsNumber(shortcut)) 
                    {

                        if( DEBUG )
                            ESP_LOGI(TAG, "Value %d", (int)shortcut->valuedouble);

                        *result = (int)shortcut->valuedouble;
                        error = 0;                                
                }

            }
        }
        cJSON_Delete(json);
                
    } 

    return error; 
}

/**
 * Função responsável em tratar os request do HTML;
 */
void http_server_netconn_serve( struct netconn *conn ) 
{    
	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;

	/**
	 * Recebe o request HTTP do client;
	 * conn é o handle da conexão e inbuf é a struct
	 * que internamente contém o buffer;
	 */
    err = netconn_recv(conn, &inbuf);
   
	if (err == ERR_OK) {
	  
	  	/**
	  	 * Os dados recebidos são populados em buf. 
	  	 * Em buflen é carregado a quantidade de bytes que foram
	  	 * recebidos e armazenados em buf; 
	  	 */
		netbuf_data(inbuf, (void**)&buf, &buflen);
		
		/**
		 * Imprime o header do request HTTP enviado pelo client; 
		 * Este debug é importante pois informa o formato do request
		 * para os diferentes navegadores utilizados, e ajuda a 
		 * identificar os requests do javascript.
		 */
		if( DEBUG ) {
			static int n_request = 0;
	        ESP_LOGI(TAG, "\r\n------------------\n");
	        ESP_LOGI(TAG, "Request N: %d: %.*s", ++n_request, buflen, buf); 
	        ESP_LOGI(TAG, "\n------------------\r\n");
    	}
        
		/**
		 * Extrai a primeira linha da string buf; 
		 * Atenção strtok altera a string. No lugar de '\n' será adicionado '\0';
		 */
		char *first_line = strtok(buf, "\n");	
		if(first_line) {
			
			/**
			 * O request padrão enviado pelo cliente quando acessado o IP do ESP8266
			 * no navegador;
			 */
			if( (strstr(first_line, "GET / ")) || (strstr(first_line, "GET /favicon.ico")) ) {
				if( DEBUG )
					ESP_LOGI(TAG, "Envia página HTML de entrada.\n");                   
                netconn_write(conn, server_html_page_form_start, server_html_page_form_end - server_html_page_form_start, NETCONN_NOCOPY);
			}

			/**
			 * Algoritmo responsável em receber os dados do
			 * formulário <form> com o login e senha de acesso ao dashboard.
			 */
            else if(strstr(first_line, "POST / ")) {
                
            	if( DEBUG )
                	ESP_LOGI(TAG, "POST / recebido.\n" ); 

                /**
                 * Separa o campo Header do Body do request post; 
                 * A separação é feita por meio do CRLFCRLF;
                 */
				for( int i = 0; i < buflen; ++i ) 
				{

					if( buf[i+0]==0x0d && buf[i+1]==0x0a && buf[i+2]==0x0d && buf[i+3]==0x0a)
					{

						/**
						 * Calcula o tamanho do conteúdo data recebido no buffer; 
						 */
						int payload_size = buflen - (&buf[i+4] - buf);

						if( DEBUG )
				  			ESP_LOGI(TAG, "Paload_size: %d", payload_size );

				  		/**
				  		 * netbuf_data retorna um vetor de char, e não uma string, sendo assim
				  		 * não é garantido o NULL no final do vetor. Portanto, será feito uma
				  		 * cópia somente do payload data do POST em um novo buffer utilizando malloc;
				  		 */
						char * payload = (char*)pvPortMalloc( payload_size + 1 );
				        if( payload == NULL ) {
				            if ( DEBUG )
				                ESP_LOGI(TAG, "Error: malloc.\n"); 
				            break;
				        }

				        /**
				         * Copia o conteúdo data recebido no request POST no buffer payload;			      
				         */
						sprintf( payload, "%.*s", payload_size, &buf[i+4]);

						/**
						 * Imprime o valor do body contendo o ssid e pwd;
						 */
						if( DEBUG )
				  			ESP_LOGI(TAG, "Payload: %s", payload);

				  		/**
				  		 * A separação dos campos do 
				  		 * content-type/x-www-form-urlencoded
				  		 * Exemplo: username=esp32&password=esp32pwd
				  		 */
				  		char delim[] = "&";

				  		/**
				  		 * Substitui o delimitador '&' por NULL;
				  		 */
				  		char *ptr = strtok( payload, delim );
				  		char user_pwd_ok = 0;
						while ( ptr != NULL )
						{
							/**
							 * Separa os campos do content-type/x-www-form-urlencoded conforme:
							 * username=esp32
							 * password=esp32pwd
							 */
							if( DEBUG )
								ESP_LOGI(TAG, "'%s'\n", ptr );

							/**
							 * Separa os campos do content-type/x-www-form-urlencoded conforme:
							 * username
							 * esp32
							 * password
							 * esp32pwd
							 * onde, ptr armazena o valor de username e esp32, e ts+1 armazena a senha e password
							 * do usuário
							 */
							char * ts = NULL; 
							if( ( ts = strstr( ptr, "=" ) ) ) 
							{
								/**
								 * Verifica se ptr é igual a string padrao username;
								 */
								if( !memcmp( ptr, "ssid", ts - ptr ) )
								{
									/**
									 * Imprime o username do formulário
									 */
									if ( DEBUG )
										ESP_LOGI(TAG, "SSID: %s\n", ts+1 );	

									/**
									 * Compara o username enviado no formulário pelo
									 * username padrão; Caso igual, seta um flag apenas, 
									 * pois tanto o username quanto o password precisam 
									 * ser iguais para chamar a pagina HTML do dashboard.
									 */

									 if( nvs_str_save("ssid", ts+1 ) == ESP_OK )
									 {
									 	user_pwd_ok = 1;
									 }
								}

								/**
								 * Verifica se ptr é igual ao password;
								 */
								if( !memcmp( ptr, "password", ts - ptr ) )
								{
									if( DEBUG )
										ESP_LOGI( TAG, "Password: %s\n", ts+1 );	

									/**
									 * Compara o password enviado no formulário pelo
									 * password padrão; Caso igual, seta um flag apenas, 
									 * pois tanto o username quanto o password precisam 
									 * ser iguais para chamar a pagina HTML do dashboard.
									 */

									if( nvs_str_save("password", ts+1 ) == ESP_OK )
									{
									 	if(user_pwd_ok)
									 	{
											if( DEBUG )
												ESP_LOGI( TAG, "ssid e password salvos em nvs.\n");	

											/**
											 * Envia um retorno para a página HTML informando
											 * o status do led0. Esse retorno é importante pois
											 * o usuário saberá se o led acendeu ou não por meio
											 * da página web. 
											 */
											char str_Number[] = "1"; 
											/**
											 * Envia o cabeçalho com a confirmação do status - code:200
											 */
											netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
											/**
											 * Envia '1' para a pagina HTML para notificar o retorno do javascript 
											 * que a gravação do ssid e password ocorreu com sucesso.;
											 */
											netconn_write(conn, str_Number, strlen(str_Number), NETCONN_NOCOPY);

											/**
											 * reseta por software o esp8266
											 */
                                            
                                            vTaskDelay(2000/portTICK_PERIOD_MS);
											ESP_ERROR_CHECK(esp_wifi_stop() );
                							ESP_ERROR_CHECK(esp_wifi_deinit() );
                							esp_restart(); 

									 		break;
									 	}
									}

								}
							}

							/**
							 * Convido o leitor a buscar mais informações na net sobre o 
							 * strtok do C; 
							 */
							ptr = strtok(NULL, delim);
						}

						if( user_pwd_ok == 0 )
						{
							/**
							 * Caso não seja possível processar ssid ou password carrega novamente
							 * a mesma página HTML do formulário;
							 */
							netconn_write( conn, server_html_page_form_start, server_html_page_form_end - server_html_page_form_start, NETCONN_NOCOPY );      
						}

						/**
						 * Desaloca o buffer que foi criado;
						 */
						vPortFree(payload);
				  		break;
					}
				}
            }
				

		}
		else {
			if( DEBUG )
				ESP_LOGI( TAG, "Request Desconhecido.\n" );
		} 
	}
	
	/**
	 * Encerra a conexão e desaloca o buffer;
	 */
	netconn_close(conn);
	netbuf_delete(inbuf);
}

/**
 * Configura o WiFi em modo Acess Point;
 */
void init_wifi_ap(void)
{
    wifi_event_group_ap = xEventGroupCreate();
    /**
     * Função para inicializar a stack TCP/IP para modo AP WiFi
     */
    start_dhcp_server();


    /*
	Registra a função de callback dos eventos do WiFi.
	*/
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = SSID_WIFI,
            .password = PWD_WIFI,
            /**
             * Número máximo de dispositivo conectado ao ESP8266.
             * Seja cauteloso, pois o ESP8266, assim como o ESP32, não é igual ao seu PC!!
             * Os recursos (memória, velocidade) desses MCUs são bem limitados;
             */
            .max_connection = 5,  	
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    if( DEBUG )
		ESP_LOGI(TAG, "Servico WiFi Acess Point configurado e inicializado. \n");
}

/**
 * Inicializa o serviço dhcp;
 */
void start_dhcp_server( void )
{  
    tcpip_adapter_init();
	/**
	 * O serviço dhcp é interrompido pois será configurado
	 * o endereço IP do ESP8266.
	 */
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

	/**
	 * Configura o endereço IP do ESP8266 na rede.
	 */
	tcpip_adapter_ip_info_t info;
	memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 1, 1);
	IP4_ADDR(&info.gw, 192, 168, 1, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	
	/* 
		Start DHCP server. Portanto, o ESP8266 fornecerá IPs aos clientes STA na 
		faixa de IP configurada acima.
	*/
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
    
    if( DEBUG )
		ESP_LOGI(TAG, "Servico DHCP configurado e inicializado. \n");
}

/**
 * @brief      Função Responsável em retornar o valor do ssid e password
 * salvos na nvs;
 *
 * @param      ssid          The ssid
 * @param[in]  ssid_len      The ssid length
 * @param      password      The password
 * @param[in]  password_len  The password length
 *
 * @return     Retorna o status da leitura da nvs;
 */
esp_err_t nvs_read_ssid_password( char * ssid, size_t ssid_len, char * password, size_t password_len )
{
    nvs_handle my_handle;
	esp_err_t err = nvs_open( "storage", NVS_READWRITE, &my_handle );
	if ( err != ESP_OK ) {					
			if( DEBUG )
				ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err );			
	} else {
		/**
		 * Leitura do SSID salvo na nvs
		 */
		err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
		if( err != ESP_OK ) {							
			if( err == ESP_ERR_NVS_NOT_FOUND ) {
				if( DEBUG )
					ESP_LOGI(TAG, "\nKey ssid not found.\n");
			} 							
		} else {
			if( DEBUG )
				ESP_LOGI(TAG, "\nssid is %s\n", ssid );
		}
		/**
		 * Leitura do Password salvo em NVS;
		 */
		err = nvs_get_str(my_handle, "password", password, &password_len);
		if( err != ESP_OK ) {							
			if( err == ESP_ERR_NVS_NOT_FOUND ) {
				if( DEBUG )
					ESP_LOGI(TAG, "\nKey password not found.\n");
			} 							
		} else {
			if( DEBUG )
				ESP_LOGI(TAG, "\npassword is %s.\n", password );
		}


		nvs_close(my_handle);		
	}

	return err;
	
}

/**
 * @brief      Função responsável pela gravação de uma string na nvs;
 *
 * @param      key    The key
 * @param      value  The value
 *
 * @return     { description_of_the_return_value }
 */
esp_err_t nvs_str_save(char * key, char * value)
{
	nvs_handle my_handle;
	esp_err_t err = nvs_open( "storage", NVS_READWRITE, &my_handle );
	if ( err != ESP_OK ) {					
			if( DEBUG )
				ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err );			
	} else {
	    err = nvs_set_str(my_handle, key, value );
        if(err != ESP_OK) {
            if( DEBUG )
                ESP_LOGI(TAG, "\nError in %s : (%04X)\n", key, err);			
        }
        /* Salva em nvs */
		err = nvs_commit(my_handle);
		if(err != ESP_OK) {
			if( DEBUG )
				printf("\nError in commit! (%04X)\n", err);
		}
									
		nvs_close(my_handle);		
	}
    
	return err;
}