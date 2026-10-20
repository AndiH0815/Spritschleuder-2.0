#pragma once
#include "esp_err.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <string>
using esp_event_base_t = int;
constexpr int ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED=123;
constexpr int WIFI_EVENT=1, IP_EVENT=2, ESP_EVENT_ANY_ID=-1;
constexpr int WIFI_EVENT_STA_DISCONNECTED=1, WIFI_EVENT_STA_STOP=2, IP_EVENT_STA_GOT_IP=3;
constexpr int WIFI_MODE_STA=1, WIFI_MODE_APSTA=3, WIFI_IF_STA=0, WIFI_IF_AP=1;
constexpr int WIFI_STORAGE_RAM=0, WIFI_AUTH_WPA2_PSK=3;
struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
struct wifi_config_t {
    struct { unsigned char ssid[32]={},password[64]={}; } sta;
    struct { unsigned char ssid[32]={},password[64]={}; unsigned char ssid_len=0,channel=0,max_connection=0; int authmode=0; } ap;
};
struct wifi_ap_record_t { unsigned char ssid[33] = {}; };
struct esp_ip4_addr_t { uint32_t addr=0; };
struct esp_netif_ip_info_t { esp_ip4_addr_t ip,gw,netmask; };
inline wifi_config_t fake_sta, fake_ap;
inline int fake_mode=WIFI_MODE_STA, fake_switches=0, fake_attempts=0;
inline bool fake_associated=false, fake_started=false;
inline int fake_wifi_error=0;
inline std::string fake_hostname;
inline int esp_netif_init(){return 0;}
inline int esp_event_loop_create_default(){return 0;}
inline void* esp_netif_create_default_wifi_sta(){return reinterpret_cast<void*>(1);}
inline void* esp_netif_create_default_wifi_ap(){return reinterpret_cast<void*>(2);}
inline int esp_netif_set_hostname(void*,const char*name){fake_hostname=name;return 0;}
inline void* esp_netif_get_handle_from_ifkey(const char*){return reinterpret_cast<void*>(1);}
inline uint32_t esp_ip4addr_aton(const char*s){return inet_addr(s);}
inline void esp_ip4addr_ntoa(const esp_ip4_addr_t*a,char*out,size_t n){inet_ntop(AF_INET,&a->addr,out,n);}
inline int esp_netif_get_ip_info(void*,esp_netif_ip_info_t*i){i->ip.addr=fake_associated?inet_addr("192.168.1.50"):0;return 0;}
inline int esp_netif_dhcps_stop(void*){return 0;}
inline int esp_netif_dhcps_start(void*){return 0;}
inline int esp_netif_set_ip_info(void*,esp_netif_ip_info_t*){return 0;}
inline int esp_wifi_init(wifi_init_config_t*){return 0;}
inline int esp_wifi_set_storage(int){return 0;}
inline int esp_event_handler_register(int,int,void(*)(void*,int,int32_t,void*),void*){return 0;}
inline int esp_wifi_set_mode(int m){fake_mode=m;++fake_switches;return fake_wifi_error;}
inline int esp_wifi_set_config(int interface,wifi_config_t*c){
    if(interface==WIFI_IF_STA)fake_sta=*c;else {assert(!fake_started);fake_ap=*c;}return fake_wifi_error;
}
inline int esp_wifi_start(){fake_started=true;return fake_wifi_error;}
inline int esp_wifi_stop(){fake_started=false;fake_associated=false;return fake_wifi_error;}
inline int esp_wifi_disconnect(){fake_associated=false;return 0;}
inline int esp_wifi_connect(){++fake_attempts;return fake_wifi_error;}
inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t*r){if(!fake_associated)return ESP_FAIL;memcpy(r->ssid,fake_sta.sta.ssid,32);return 0;}
inline void vTaskDelay(int){}
