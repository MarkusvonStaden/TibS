#ifndef __WIFI_H__
#define __WIFI_H__

#define WIFI_SSID     "Heimatwinkel WG"
#define WIFI_PASSWORD "H4w4iiPi$$4"

#define BASE_URL "http://e795af42-f489-4f54-8ff6-ade24852e1da.ul.bw-cloud-instance.org"

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
void send_data(double* moisture, double* temperature, double* humidity, double* pressure, double* white, double* visible, char* version);
#endif