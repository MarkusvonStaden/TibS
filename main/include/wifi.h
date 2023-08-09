// Import protection
#ifndef __WIFI_H__
#define __WIFI_H__

void wifi_init_sta(void);
void send_data(double* temperature, double* humidity, double* pressure, double* white, double* visible);
#endif  // __WIFI_H__