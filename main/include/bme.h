#ifndef BME_280_I2C_H
#define BME_280_I2C_H

#include <stdint.h>

#include "bme280_defs.h"

int8_t BME280_I2C_bus_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t cnt, void *interface);
int8_t BME280_I2C_bus_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t cnt, void *interface);
void   BME280_delay_usek(uint32_t msek, void *interface);
void   BME_init_wrapper();
int8_t BME_force_read(double *temperature, double *pressure, double *humidity);

#endif