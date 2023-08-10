/* Static IP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include <netdb.h>
#include "nvs_flash.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* The examples use configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
//wifi marco
// #define EXAMPLE_WIFI_SSID             "Liz-5G"
// #define EXAMPLE_WIFI_PASS             "Liz202204"
#define EXAMPLE_WIFI_SSID             "ESP_32"
#define EXAMPLE_WIFI_PASS             "adminadmin"
#define EXAMPLE_MAXIMUM_RETRY         5
#define EXAMPLE_STATIC_IP_ADDR        "192.168.1.140"
#define EXAMPLE_STATIC_NETMASK_ADDR   "255.255.255.0"
#define EXAMPLE_STATIC_GW_ADDR        "192.168.1.1"
#ifdef CONFIG_EXAMPLE_STATIC_DNS_AUTO
#define EXAMPLE_MAIN_DNS_SERVER       EXAMPLE_STATIC_GW_ADDR
#define EXAMPLE_BACKUP_DNS_SERVER     "192.168.1.1"
#define portTICK_RATE_MS              portTICK_PERIOD_MS

#define HOST_IP_ADDR "192.168.1.254"
#define PORT 7789
#define UDP_PORT1 7790

//uart marco
static const int RX_BUF_SIZE = 1024;
//static const int TX_BUF_SIZE = 64;
#define TXD_PIN (GPIO_NUM_25)
#define RXD_PIN (GPIO_NUM_34)
#define UART_NUM UART_NUM_1

// #define TXD_PIN (GPIO_NUM_20)
// #define RXD_PIN (GPIO_NUM_21)
// #define UART_NUM UART_NUM_1
                                                                                
#else
#define EXAMPLE_MAIN_DNS_SERVER       CONFIG_EXAMPLE_STATIC_DNS_SERVER_MAIN
#define EXAMPLE_BACKUP_DNS_SERVER     CONFIG_EXAMPLE_STATIC_DNS_SERVER_BACKUP
#endif
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
#define EXAMPLE_RESOLVE_DOMAIN        CONFIG_EXAMPLE_STATIC_RESOLVE_DOMAIN
#endif
//non blocking_socket
#define INVALID_SOCK (-1)
#define YIELD_TO_ALL_MS 20

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows mult  iple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "TCP_Client_NON_Blocking";

const char COM_Recular[5] =  {0xFF, 0xFF, 0xAA, 0x00, 0x11};
const char COM_Cali_On[5] =  {0xFF, 0xFF, 0xAA, 0x00, 0xAA};
const char COM_Cali_Off[5] = {0xFF, 0xFF, 0xAA, 0x00, 0xFF};

int totalnum[11] = {0};
int tmpnum[11] = {0};

static int s_retry_num = 0;

static esp_err_t example_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static void example_set_static_ip(esp_netif_t *netif)
{
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(EXAMPLE_STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(EXAMPLE_STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(EXAMPLE_STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGD(TAG, "Success to set static ip: %s, netmask: %s, gw: %s", EXAMPLE_STATIC_IP_ADDR, EXAMPLE_STATIC_NETMASK_ADDR, EXAMPLE_STATIC_GW_ADDR);
    ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr(EXAMPLE_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
    ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr(EXAMPLE_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        example_set_static_ip(arg);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}



static void log_socket_error(const char *tag, const int sock, const int err, const char *message)
{
    ESP_LOGE(tag, "[sock=%d]: %s\n"
                  "error=%d: %s", sock, message, err, strerror(err));
}

static int try_receive(const char *tag, const int sock, char * data, size_t max_len)
{
    int len = recv(sock, data, max_len, 0);
    if (len < 0) {
        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   // Not an error
        }
        if (errno == ENOTCONN) {
            ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
            return -2;  // Socket has been disconnected
        }
        log_socket_error(tag, sock, errno, "Error occurred during receiving");
        return -1;
    }

    return len;
}
static int try_receive_udp(const char *tag, const int sock, char * data, size_t max_len)
{  
    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(source_addr);
    int len = recvfrom(sock, data, max_len, 0, (struct sockaddr *)&source_addr, &socklen);
    if (len < 0) {
        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   // Not an error
        }
        if (errno == ENOTCONN) {
            ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
            return -2;  // Socket has been disconnected
        }
        log_socket_error(tag, sock, errno, "Error occurred during receiving");
        return -1;
    }

    return len;
}

static int socket_send(const char *tag, const int sock, const char * data, const size_t len)
{
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
            log_socket_error(tag, sock, errno, "Error occurred during sending");
            return -1;
        }
        to_write -= written;
    }
    return len;
}

static void udp_client_task(void *pvParameters){
    char rx_buffer[128];
    char host_ip[] = "255.255.255.255";
    int addr_family = 0;
    int ip_protocol = 0;
    static const char *TAG = "UDP_CLIENT";
    char* data = (char*) malloc(RX_BUF_SIZE+1);

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);

        
        
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        struct sockaddr_in src_addr;
        src_addr.sin_addr.s_addr = inet_addr(EXAMPLE_STATIC_IP_ADDR);
        src_addr.sin_family = AF_INET;
        src_addr.sin_port = htons(UDP_PORT1);
        if (bind(sock,(struct sockaddr *)&src_addr,sizeof(src_addr)) != 0 )
        { log_socket_error(TAG, sock, errno, "Unable to bind udp_port"); }

        int flags = fcntl(sock, F_GETFL);
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            log_socket_error(TAG, sock, errno, "Unable to set socket non blocking");
        }   
        // Set timeout
        // struct timeval timeout;
        // timeout.tv_sec = 10;
        // timeout.tv_usec = 0;
        // setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", host_ip, PORT);

        while (1) {
            const int rxBytes = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 10 / portTICK_PERIOD_MS);
            if (rxBytes > 0) 
            {
                ESP_LOGI(TAG, "Read %d bytes.", rxBytes);
                int err = sendto(sock, data, rxBytes, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                ESP_LOGI(TAG, "Message sent");
            }

            
            // int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            //  接收上位机的指令
            int len = try_receive_udp(TAG, sock, rx_buffer, sizeof(rx_buffer));
            vTaskDelay(YIELD_TO_ALL_MS/portTICK_PERIOD_MS);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else if(len>0){
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if(len==5){
                    if(memcmp(&rx_buffer, &COM_Cali_On, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send calibration start command.");
                    }
                    else if(memcmp(rx_buffer, COM_Cali_Off, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send calibration end command.");
                    }
                    else if(memcmp(rx_buffer, COM_Recular, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send recular working command.");
                    }
                    else{
                        ESP_LOGE(TAG, "Received Wrong command! Please send again!");
                    }
                }
                
                // if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                //     ESP_LOGI(TAG, "Received expected message, reconnecting");
                //     break;
                // }
            }

           // vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    free(data);
    vTaskDelete(NULL);
}

static void tcp_client_task(void *pvParameters)
{
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;
    int res = -1;
    int len = 0;
    static char rx_buffer[128];

    char* data = (char*) malloc(RX_BUF_SIZE+1);

    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = INVALID_SOCK;
    while(1){
        sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            log_socket_error(TAG, sock, errno, "Unable to create socket");
            goto error;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);
        // ESP_LOGI(TAG, "Socket created, connecting to %s:%s", CONFIG_EXAMPLE_TCP_CLIENT_CONNECT_ADDRESS, CONFIG_EXAMPLE_TCP_CLIENT_CONNECT_PORT);


        //***************** Marking the socket as non-blocking ***************//
        int flags = fcntl(sock, F_GETFL);
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            log_socket_error(TAG, sock, errno, "Unable to set socket non blocking");
        }

        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
            if (errno == EINPROGRESS) {
                ESP_LOGD(TAG, "connection in progress");
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(sock, &fdset);

                // Connection in progress -> have to wait until the connecting socket is marked as writable, i.e. connection completes
                res = select(sock+1, NULL, &fdset, NULL, NULL);
                if (res < 0) {
                    log_socket_error(TAG, sock, errno, "Error during connection: select for socket to be writable");
                    goto error;
                } else if (res == 0) {
                    log_socket_error(TAG, sock, errno, "Connection timeout: select for socket to be writable");
                    goto error;
                } else {
                    int sockerr;
                    socklen_t len = (socklen_t)sizeof(int);

                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                        log_socket_error(TAG, sock, errno, "Error when getting socket error using getsockopt()");
                        goto error;
                    }
                    if (sockerr) {
                        log_socket_error(TAG, sock, sockerr, "Connection error");
                        goto error;
                    }
                }
            } else {
                log_socket_error(TAG, sock, errno, "Socket is unable to connect");
                goto error;
            }
        }
        //防止tcp粘包，TCP_NODELAY，关闭Nagle算法，有包就直接发
        int rc = 0;
        int nodelay = 1;
        rc = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
        // int cork = 1;
        // rc = setsockopt(sock, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));


        while(1){
            // 向上位机发送惯导和鞋垫数据
            const int rxBytes = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 10 / portTICK_PERIOD_MS);
            if (rxBytes > 0) 
            {
                ESP_LOGI(TAG, "Read %d bytes.", rxBytes);
                int len = socket_send(TAG, sock, data,rxBytes);

                for(int ty=0;ty<rxBytes-4;ty++){
                    if(data[ty] == 0xBB && data[ty+1]==0xBB && data[ty+2]==0x00 && data[ty+4]==0x18){
                        int tt = data[ty+3];
                        totalnum[tt]++;
                    }
                    if(data[ty] == 0xDD && data[ty+1]==0xDD && data[ty+2]==0x00 && data[ty+4]==0x10){
                        int tt = data[ty+3];
                        totalnum[tt+9]++;
                    }
                }

                memset(data, 0, rxBytes);
                // tmp_num_count += rxBytes;
                // if(tmp_num_count >2000){
                //     send_num_count += tmp_num_count;
                //     ESP_LOGI(TAG, "Read %d bytes this round, %d bytes total.", tmp_num_count, send_num_count);
                //     tmp_num_count = 0;
                // }
                if (len < 0) {
                    ESP_LOGE(TAG, "Error occurred during socket_send");
                    goto error;
                }
            }

            //  接收上位机的指令
            len = try_receive(TAG, sock, rx_buffer, sizeof(rx_buffer));
            vTaskDelay(YIELD_TO_ALL_MS/portTICK_PERIOD_MS);
            
            //ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
            if (len < 0) {
                ESP_LOGE(TAG, "Error occurred during try_receive");
                goto error;
            }
            else if(len > 0){
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if(len==5){
                    if(memcmp(&rx_buffer, &COM_Cali_On, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send calibration start command.");
                    }
                    else if(memcmp(rx_buffer, COM_Cali_Off, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send calibration end command.");
                    }
                    else if(memcmp(rx_buffer, COM_Recular, 5)==0){
                        uart_write_bytes(UART_NUM, rx_buffer, len);
                        ESP_LOGI(TAG, "Succeed send recular working command.");
                    }
                    else{
                        ESP_LOGE(TAG, "Received Wrong command! Please send again!");
                    }
                }
                else{
                    ESP_LOGE(TAG, "Received Wrong data! Please send again!");
                }
                memset(rx_buffer, 0, len);
            }
        }
    error:
        if (sock != INVALID_SOCK) {
            close(sock);
        }
    }
    free(data);
    vTaskDelete(NULL);
}


void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}


void RxIMUDataCount(void){
    while(1){
        vTaskDelay(10000/portTICK_PERIOD_MS);
        for(int i=0;i<10;i++){
            tmpnum[i] = totalnum[i] - tmpnum[i]; 
        }
        ESP_LOGI(TAG, "totalnum:00:%d,  01:%d, 02:%d,  03:%d, 04:%d, 05:%d,  06:%d, 07:%d, 08:%d, LXD: %d, RXD:%d", totalnum[0],totalnum[1],totalnum[2],totalnum[3],totalnum[4],totalnum[5],totalnum[6],totalnum[7],totalnum[8],totalnum[9],totalnum[10]);
        ESP_LOGI(TAG, "thisroundnum:00:%d,  01:%d, 02:%d,  03:%d, 04:%d, 05:%d,  06:%d, 07:%d, 08:%d, LXD: %d, RXD:%d", tmpnum[0],tmpnum[1],tmpnum[2],tmpnum[3],tmpnum[4],tmpnum[5],tmpnum[6],tmpnum[7],tmpnum[8],tmpnum[9],tmpnum[10]);
        for(int i=0;i<10;i++){
            tmpnum[i] = totalnum[i]; 
        }
    }
                                                                             
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    uart_init();
    ESP_LOGI(TAG, "UART");

    wifi_init_sta();
    // xTaskCreate(tcp_client_task, "tcp_client_task", 4096, NULL, 5, NULL);
    // xTaskCreate(RxIMUDataCount, "RxIMUDataCount",4096, NULL, 6, NULL);
    xTaskCreate(udp_client_task, "udp_client_task", 4096, NULL, 5, NULL);
}
