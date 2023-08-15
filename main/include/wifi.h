// Import protection
#ifndef __WIFI_H__
#define __WIFI_H__

typedef struct {
    double min;
    double max;
} Range;

typedef struct {
    Range moisture;
    Range temperature;
    Range humidity;
    Range pressure;
    Range white;
    Range visible;
} Limits;

void wifi_init_sta(void);
void send_data(double* moisture, double* temperature, double* humidity, double* pressure, double* white, double* visible);
#endif  // __WIFI_H__