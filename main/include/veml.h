#ifndef VEML_3235_H
#define VEML_3235_H

#include <stdint.h>

#include "esp_err.h"

esp_err_t VEML_init();
esp_err_t VEML_read(double *white, double *visible);

#endif