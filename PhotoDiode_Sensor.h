#ifndef DIODE_SENSOR_H
#define DIODE_SENSOR_H

#include <stdbool.h>
void diode_sensor_init(void);
unsigned int diode_sensor_read_result(unsigned int *res);
void diode_sensor_read(void);

#endif
