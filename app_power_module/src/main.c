/*
 * Copyright (c) 2023 Aaron Chan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_module_defs.h"

#include <launch_core/backplane_defs.h>

#include <launch_core/dev/adc.h>
#include <launch_core/dev/dev_common.h>
#include <launch_core/dev/sensor.h>

#include <launch_core/net/net_common.h>
#include <launch_core/net/udp.h>

#include <zephyr/drivers/gpio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define SENSOR_READ_STACK_SIZE (512)
#define QUEUE_PROCESSING_STACK_SIZE (1024)
#define INA219_UPDATE_TIME_MS (67)

LOG_MODULE_REGISTER(main, CONFIG_APP_POWER_MODULE_LOG_LEVEL);

static struct k_msgq ina_processing_queue;
static uint8_t ina_processing_queue_buffer[CONFIG_INA219_QUEUE_SIZE * sizeof(power_module_telemetry_t)];

static K_THREAD_STACK_DEFINE(ina_read_stack, SENSOR_READ_STACK_SIZE);
static struct k_thread ina_read_thread;

static K_THREAD_STACK_DEFINE(ina_processing_stack, QUEUE_PROCESSING_STACK_SIZE);
static struct k_thread ina_processing_thread;

static const struct device *const wiznet = DEVICE_DT_GET_ONE(wiznet_w5500);

//static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
//static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
//static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
//static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);
//static const struct gpio_dt_spec led_wiznet = GPIO_DT_SPEC_GET(DT_ALIAS(ledwiz), gpios);

static const enum sensor_channel ina_channels[] = {
        SENSOR_CHAN_CURRENT,
        SENSOR_CHAN_VOLTAGE,
        SENSOR_CHAN_POWER
};

static const float ADC_GAIN = 0.09f;
static const float MV_TO_V_MULTIPLIER = 0.001f;


static void ina_task(void *, void *, void *) {
    power_module_telemetry_t sensor_telemetry = {0};
    int16_t vin_adc_data = 0;
    uint16_t temp_vin_adc_data = 0;

    const struct device *sensors[] = {
            DEVICE_DT_GET(DT_ALIAS(inabatt)), // Battery
            DEVICE_DT_GET(DT_ALIAS(ina3v3)), // 3v3
            DEVICE_DT_GET(DT_ALIAS(ina5v0)) // 5v0
    };

    const struct adc_dt_spec vin_sense_adc = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

    struct adc_sequence vin_sense_sequence = {
            .buffer = &temp_vin_adc_data,
            .buffer_size = sizeof(temp_vin_adc_data),
    };

    const bool ina_device_found[] = {
            l_check_device(sensors[0]) == 0,
            l_check_device(sensors[1]) == 0,
            l_check_device(sensors[2]) == 0
    };

    const bool adc_ready = l_init_adc_channel(&vin_sense_adc, &vin_sense_sequence) == 0;

    if (!ina_device_found[0]) {
        LOG_ERR("INA219 battery sensor not found");
    }

    if (!ina_device_found[1]) {
        LOG_ERR("INA219 3v3 sensor not found");
    }

    if (!ina_device_found[2]) {
        LOG_ERR("INA219 5v0 sensor not found");
    }

    if (!adc_ready) {
        LOG_ERR("ADC channel %d is not ready", vin_sense_adc.channel_id);
        sensor_telemetry.vin_adc_data_mv = -0x7FFF;

    }

    while (true) {
        l_update_sensors_safe(sensors, 3, ina_device_found);
        if (likely(adc_ready)) {
            if (0 <= l_read_adc_mv(&vin_sense_adc, &vin_sense_sequence, (int32_t * ) & vin_adc_data)) {
                sensor_telemetry.vin_adc_data_mv = vin_adc_data;
            } else {
                LOG_ERR("Failed to read ADC value from %d", vin_sense_adc.channel_id);
                sensor_telemetry.vin_adc_data_mv = -0x7FFF;
            }
        }
        sensor_telemetry.timestamp = k_uptime_get_32();

        struct sensor_value current;
        struct sensor_value voltage;
        struct sensor_value power;
        struct sensor_value *sensor_values[] = {
                &current,
                &voltage,
                &power
        };

        if (likely(ina_device_found[0])) {
            l_get_sensor_data(sensors[0], 3, ina_channels, sensor_values);
            sensor_telemetry.data_battery.current = sensor_value_to_float(&current);
            sensor_telemetry.data_battery.voltage = sensor_value_to_float(&voltage);
            sensor_telemetry.data_battery.power = sensor_value_to_float(&power);
        }

        if (likely(ina_device_found[1])) {
            l_get_sensor_data(sensors[1], 3, ina_channels, sensor_values);
            sensor_telemetry.data_3v3.current = sensor_value_to_float(&current);
            sensor_telemetry.data_3v3.voltage = sensor_value_to_float(&voltage);
            sensor_telemetry.data_3v3.power = sensor_value_to_float(&power);
        }

        if (likely(ina_device_found[2])) {
            l_get_sensor_data(sensors[2], 3, ina_channels, sensor_values);
            sensor_telemetry.data_5v0.current = sensor_value_to_float(&current);
            sensor_telemetry.data_5v0.voltage = sensor_value_to_float(&voltage);
            sensor_telemetry.data_5v0.power = sensor_value_to_float(&power);
        }

        if (k_msgq_put(&ina_processing_queue, &sensor_telemetry, K_NO_WAIT)) {
            LOG_ERR("Failed to put data into INA219 processing queue");
        }

        // Wait some time for sensor to get new values (15 Hz -> 66.67 ms)
        uint32_t time_to_wait = INA219_UPDATE_TIME_MS - (k_uptime_get_32() - sensor_telemetry.timestamp);
        if (time_to_wait > 0) {
            k_sleep(K_MSEC(time_to_wait));
        }
    }
}

static void ina_queue_processing_task(void *, void *, void *) {
    power_module_telemetry_t sensor_telemetry = {0};
    power_module_telemetry_packed_t packed_telemetry = {0};

    while (true) {
        if (k_msgq_get(&ina_processing_queue, &sensor_telemetry, K_FOREVER)) {
            LOG_ERR("Failed to get data from INA219 processing queue");
            continue;
        }

        packed_telemetry.current_battery = sensor_telemetry.data_battery.current;
        packed_telemetry.voltage_battery = sensor_telemetry.data_battery.voltage;
        packed_telemetry.power_battery = sensor_telemetry.data_battery.power;

        packed_telemetry.current_3v3 = sensor_telemetry.data_3v3.current;
        packed_telemetry.voltage_3v3 = sensor_telemetry.data_3v3.voltage;
        packed_telemetry.power_3v3 = sensor_telemetry.data_3v3.power;

        packed_telemetry.current_5v0 = sensor_telemetry.data_5v0.current;
        packed_telemetry.voltage_5v0 = sensor_telemetry.data_5v0.voltage;
        packed_telemetry.power_5v0 = sensor_telemetry.data_5v0.power;

        packed_telemetry.vin_adc_data_v = (sensor_telemetry.vin_adc_data_mv * MV_TO_V_MULTIPLIER) * ADC_GAIN;

        // TODO: write to flash when data logging library is ready
        l_send_udp_broadcast((uint8_t * ) & packed_telemetry, sizeof(power_module_telemetry_packed_t),
                             POWER_MODULE_BASE_PORT + POWER_MODULE_INA_DATA_PORT);
    }
}

static int init(void) {
    char ip[MAX_IP_ADDRESS_STR_LEN];
    int ret = -1;

    k_msgq_init(&ina_processing_queue, ina_processing_queue_buffer, sizeof(power_module_telemetry_t),
                CONFIG_INA219_QUEUE_SIZE);
    if (0 > l_create_ip_str_default_net_id(ip, POWER_MODULE_ID, 1)) {
        LOG_ERR("Failed to create IP address string: %d", ret);
        return -1;
    }

    if (!l_check_device(wiznet)) {
        ret = l_init_udp_net_stack(ip);
        if (ret != 0) {
            LOG_ERR("Failed to initialize network stack");
            return ret;
        }
    } else {
        LOG_ERR("Failed to get network device");
        return ret;
    }

    // TODO: Play with these values on rev 2 where we can do more profiling
    k_thread_create(&ina_read_thread, &ina_read_stack[0], SENSOR_READ_STACK_SIZE, ina_task, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
    k_thread_start(&ina_read_thread);

    k_thread_create(&ina_processing_thread, &ina_processing_stack[0], QUEUE_PROCESSING_STACK_SIZE,
                    ina_queue_processing_task, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0,
                    K_NO_WAIT);
    k_thread_start(&ina_processing_thread);

    return 0;
}


int main(void) {
    if (init()) {
        return -1;
    }

    while (true) {
        k_sleep(K_MSEC(100));
    }

    return 0;

}