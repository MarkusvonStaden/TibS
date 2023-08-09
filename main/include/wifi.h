// Import protection
#ifndef __WIFI_H__
#define __WIFI_H__

void wifi_init_sta(void);
void send_data(uint32_t* temperature, uint32_t* humidity, uint32_t* pressure);
#endif  // __WIFI_H__