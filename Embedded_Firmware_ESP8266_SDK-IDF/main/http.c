#include "http.h"

#define DEBUG 1
static const char *TAG = "web-server";

/**
 * A função a seguir tem o objetivo de enviar mensagens GET, POST, DELETE via
 * http;
 */
signed int http_send( http_config_t * config )
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int socket, r;
    int err;
                           
    /**
     * Obtem informações do DNS do domínio informado.
     */
    err = getaddrinfo( config->ip, config->port, &hints, &res);
    if(err != 0 || res == NULL) {
        if( DEBUG )
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        
        /**
         * Error - Não foi possível obter informações do DNS.
         */
        freeaddrinfo(res);
        return E_DNS_FAILED;
    }

    /**
     * Imprime o endereço IP para do DNS informado. 
     */
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    if( DEBUG )
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    /**
     * Cria o Socket HTTP.
     */
    socket = socket(res->ai_family, res->ai_socktype, 0);
    if(socket < 0) {
        
        if( DEBUG )
            ESP_LOGE(TAG, "... Failed to allocate socket.");

        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        return E_SOCKET_FAILED;
    }
    
    if( DEBUG )
        ESP_LOGI(TAG, "... allocated socket");

    /**
     * Tenta Conectar o ESP8266 ao servidor web.
     */
    if(connect(socket, res->ai_addr, res->ai_addrlen) != 0) {
        
        if( DEBUG )
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);

        close(socket);
        freeaddrinfo(res);
        vTaskDelay(4000 / portTICK_PERIOD_MS);

        return E_CONNECT_FAILED;
    }
    
    if( DEBUG )
        ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);


    /**
     * Envia para o Servidor WEB o request GET;
     */
    if (write(socket, config->url, strlen(config->url)) < 0) {
        
        if( DEBUG )
            ESP_LOGE(TAG, "... socket send failed");

        close( socket );
        vTaskDelay(4000 / portTICK_PERIOD_MS);
       
        return E_SEND_FAILED ;
    }
    
    if( DEBUG )
        ESP_LOGI(TAG, "... socket send success");

    /**
     * Define um timeout de 5 segundos na configuração do socket. Caso não consiga configurar
     * o socket, será retornado E_SOCKET_TIMEOUT.
     */
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
        
        if( DEBUG )    
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");

        close( socket );
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        
        return E_SOCKET_TIMEOUT;
    }
    
    if( DEBUG )
        ESP_LOGI(TAG, "... set socket receiving timeout success");

    /**
     * Recebe a mensagem de retorno do servidor Web e printa pela serial. 
     * Como foi estabelecida na configuração acima o timeout de 5 segundos, logo a recepção dos
     * dados será interrompida após 5 segundos, ou quando r receber 0 ou -1 em (r = read(...));
     */
    /* Read HTTP response */
    int resp_offset = 0;

    /**
     * Zera o buffer de recepção http
     */
    bzero(config->http_buffer, config->http_buffer_size);
    config->http_buffer_qtd_bytes = 0;
    do {

        /**
         * Lê os bytes recebidos e armazena no http_buffer. 
         * É preciso entender que em http as informações podem chegar fragmentadas. Sendo assim, 
         * é importante ir adicionando ao buffer cada parte da mensagem recebida em http_buffer. 
         * Quando r assumir -1, significa que não foi possível ler o buffer do stack tcp. 
         * A variável r armazena a quantidade de bytes que foram recebidos e armazenados em http_buffer.
         */
        r = read( socket, config->http_buffer + resp_offset, config->http_buffer_size - resp_offset );
        
        if ( DEBUG ) 
        {
            for(int i = resp_offset; i < r+resp_offset; i++) {
                putchar(config->http_buffer[i]);
            }
        }
        /**
         * Verifica se houve erro na leitura do buffer do stack tcp;
         */
        if( r == -1)
        {
            close(socket);
            return E_SOCKET_READ;
        }

        /**
         * A variável r armazena a quantidade de bytes que foram recebidos e armazenados em http_buffer.
         */
        if( r > 0 )
        {
            resp_offset += r; 
            config->http_buffer_qtd_bytes = resp_offset;
        }
        
        /**
         * Quando r = 0, logo foi finalizado a comunicação pelo servidor web.
         */
    } while(r > 0);
    config->http_buffer[resp_offset]=0; //Adiciona o caractere null para formar uma string.
    /**
     * Fecha a conexão socket.
     */
    close(socket);
    
    if( DEBUG )
        ESP_LOGI(TAG, "Fim da Conexao Socket.");

    return E_SEND_OK;
    
}