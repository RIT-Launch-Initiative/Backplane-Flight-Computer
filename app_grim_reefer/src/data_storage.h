#ifndef REEFER_INLCUDE_DATA_STORAGE_H
#define REEFER_INLCUDE_DATA_STORAGE_H

#include <stdint.h>
#include <zephyr/kernel.h>

struct slow_data {
    uint32_t timestamp;

    uint32_t humidity;
    uint32_t temperature;

    uint16_t grim_voltage; // LSB =  1.25 mV
    uint16_t grim_current; // LSB =  1.25 mA

    uint16_t load_cell_voltage; // LSB =  1.25 mV
    uint16_t load_cell_current; // LSB =  1.25 mA

    uint16_t bat_voltage; // LSB =  1.25 mV
    uint16_t bat_current; // LSB =  1.25 mA
};

struct fast_data {
    uint32_t timestamp;

    uint32_t accel_x;
    uint32_t accel_y;
    uint32_t accel_z;

    uint32_t gyro_x;
    uint32_t gyro_y;
    uint32_t gyro_z;
};

struct adc_data {
    uint32_t timestamp;
    uint32_t adc_value[10];
};

#define STORAGE_SETUP_SUCCESS_EVENT 0x1
#define STORAGE_SETUP_FAILED_EVENT  0x2
extern struct k_event storage_setup_finished;

k_tid_t spawn_data_storage_thread();

#endif