#ifdef HCI
#include <esp_bt.h>
#include <esp32-hal-log.h>
#include <esp32-hal-bt.h>
#include <nvs.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

#include "hci_transport.h"

//=============================================================================================
//=============================================================================================
//  hci transport for esp32

// wrapper for vhci
struct {
    hci_on_packet_handler handler;
    void* handler_ref;
    hci_on_ready_to_send_handler ready_handler;
    void* ready_handler_ref;
    esp_vhci_host_callback_t _cb;
} _hci_transport;


hci_handle hci_open()
{
    if (!btStart()) {
        log_e("btStart failed");
        return NULL;
    }

    _hci_transport._cb.notify_host_recv = [](uint8_t *data, uint16_t len) -> int {
        if (_hci_transport.handler)
            _hci_transport.handler(&_hci_transport,data,len,_hci_transport.handler_ref);
    };
    _hci_transport._cb.notify_host_send_available = []() {
        if (_hci_transport.ready_handler)
            _hci_transport.ready_handler(&_hci_transport,_hci_transport.ready_handler_ref);
    };

    esp_vhci_host_register_callback(&_hci_transport._cb); // open for buisness
    return &_hci_transport;
}

int hci_close(hci_handle h)
{
    btStop();
    return 0;
}

void hci_set_packet_handler(hci_handle h, hci_on_packet_handler p, void* ref)
{
    _hci_transport.handler = p;
    _hci_transport.handler_ref = ref;
}

void hci_set_ready_to_send_handler(hci_handle h, hci_on_ready_to_send_handler p, void* ref)
{
    _hci_transport.ready_handler = p;
    _hci_transport.ready_handler_ref = ref;
}

int  hci_send(hci_handle h, const uint8_t* data, int len)
{
    esp_vhci_host_send_packet((uint8_t*)data,len);
}

int  hci_send_available(hci_handle h)
{
    return esp_vhci_host_check_send_available();
}

// store and load link keys
uint32_t _nvs_handle = 0;
static uint32_t open_nvs()
{
    nvs_open("esp_8_bit", NVS_READWRITE, &_nvs_handle);
    if (!_nvs_handle)
        printf("_nvs_handle open failed!\n");
#if 0
    nvs_iterator_t it = nvs_entry_find("nvs", "esp_8_bit", NVS_TYPE_ANY);
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it);
        printf("key '%s', type '%d' \n", info.key, info.type);
    };
#endif
    return _nvs_handle;
}

int sys_get_pref(const char* key, char* value, int max_len)
{
    value[0] = 0;
    uint32_t h = open_nvs();
    if (!h)
        return 0;
    size_t s = max_len;
    if (ESP_OK == nvs_get_str(h, key, value, &s)) {
       // printf("sys_get_pref %s:%s %d\n",key,value,(int)s);
        return (int)s;
    }
    return 0;
}

void sys_set_pref(const char* key, const char* value)
{
    uint32_t h = open_nvs();
    if (!h)
        return;
    //printf("sys_set_pref %s:%s\n",key,value);
    if (ESP_OK == nvs_set_str(h, key, value))
        nvs_commit(h);
    else
        printf("sys_set_pref %s:%s failed (key length <= 15?)\n",key,value);
}
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>
extern "C" {
#include "ff.h"
}

#define KEY_LEN 256
#define VAL_LEN 256

// store and load link keys
FIL _nvs_handle;
static bool open_nvs()
{
    if (f_open(&_nvs_handle, "pico_8_bit.nvs", FA_READ | FA_WRITE | FA_OPEN_ALWAYS) != FR_OK) {
        printf("_nvs_handle open failed!\n");
        return false;
    }
    return true;
}

int sys_get_pref(const char* key, char* value, int max_len)
{
    value[0] = 0;
    bool h = open_nvs();
    if (!h)
        return 0;
    char ks[KEY_LEN];
    char vs[VAL_LEN];
    UINT rb;
    while(1) {
        if (f_read(&_nvs_handle, ks, KEY_LEN, &rb) != FR_OK) {
            f_close(&_nvs_handle);
            return 0;
        }
        if (f_read(&_nvs_handle, vs, VAL_LEN, &rb) != FR_OK) {
            f_close(&_nvs_handle);
            return 0;
        }
        if (strncmp(key, ks, KEY_LEN > max_len ? max_len : KEY_LEN) == 0) {
            f_close(&_nvs_handle);
            strncpy(value, vs, VAL_LEN > max_len ? max_len : VAL_LEN);
            printf("sys_get_pref %s:%s %d\n",key,value,rb);
            return (int)rb;
        }
    }
    return 0;
}

void sys_set_pref(const char* key, const char* value)
{
    bool h = open_nvs();
    if (!h)
        return;
    char vs[VAL_LEN];
    UINT wb;
    while(f_size(&_nvs_handle) > f_tell(&_nvs_handle)) {
        if (f_read(&_nvs_handle, vs, VAL_LEN, &wb) != FR_OK) {
            f_close(&_nvs_handle);
            return;
        }
        if (strncmp(key, vs, VAL_LEN) == 0) {
            strncpy(vs, value, VAL_LEN);
            f_write(&_nvs_handle, vs, VAL_LEN, &wb);
            f_close(&_nvs_handle);
            printf("sys_set_pref %s:%s \n",key,value);
            return;
        }
        if (f_read(&_nvs_handle, vs, VAL_LEN, &wb) != FR_OK) {
            f_close(&_nvs_handle);
            return;
        }
    }
    strncpy(vs, key, 256);
    f_write(&_nvs_handle, vs, 256, &wb);
    strncpy(vs, value, 256);
    f_write(&_nvs_handle, vs, 256, &wb);
    f_close(&_nvs_handle);
    printf("sys_set_pref %s:%s failed (key length <= 15?)\n",key,value);
}
