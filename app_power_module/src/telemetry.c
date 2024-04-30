#include <launch_core/dev/adc.h>
#include <launch_core/dev/dev_common.h>
#include <launch_core/dev/sensor.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#define INA219_UPDATE_TIME_MS       (67)
#define ADC_UPDATE_TIME_MS          (15)
#define SENSOR_READ_STACK_SIZE      (640)

LOG_MODULE_REGISTER(telemetry);

static void ina_task(void *, void *, void *);

static void adc_task(void *, void *, void *);

K_THREAD_DEFINE(ina_thread, SENSOR_READ_STACK_SIZE, ina_task, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, 1000);
K_THREAD_DEFINE(adc_thread, SENSOR_READ_STACK_SIZE, adc_task, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, 1000);

K_MSGQ_DEFINE(ina_telemetry_msgq, sizeof(power_module_telemetry_t), 10, 4);
K_MSGQ_DEFINE(adc_telemetry_msgq, sizeof(float), 10, 4);

K_TIMER_DEFINE(ina_task_timer, NULL, NULL);
K_TIMER_DEFINE(adc_task_timer, NULL, NULL);

static bool init_ina_task(const struct device *sensors[3], bool ina_device_found[3]) {
    const char *sensor_names[] = {"Battery", "3v3", "5v0"};
    int error_count = 0;

    for (int i = 0; i < 3; i++) {
        ina_device_found[i] = l_check_device(sensors[i]) == 0;
        if (!ina_device_found[i]) {
            LOG_ERR("INA219 %s sensor not found", sensor_names[i]);
            error_count++;
        }
    }

    if (error_count == 3) {
        return false;
    }

    k_timer_start(&ina_task_timer, K_MSEC(INA219_UPDATE_TIME_MS), K_MSEC(INA219_UPDATE_TIME_MS));
    return true;
}

static bool init_adc_task(const struct adc_dt_spec *p_vin_sense_adc, struct adc_sequence *p_vin_sense_sequence) {
    const bool adc_ready = l_init_adc_channel(p_vin_sense_adc, p_vin_sense_sequence) == 0;

    if (!adc_ready) {
        LOG_ERR("ADC channel %d is not ready", p_vin_sense_adc->channel_id);
    } else {
        k_timer_start(&ina_task_timer, K_MSEC(ADC_UPDATE_TIME_MS), K_MSEC(ADC_UPDATE_TIME_MS));
    }

    return adc_ready;
}

static void ina_task(void *, void *, void *) {
    const struct device *sensors[] = {
            DEVICE_DT_GET(DT_ALIAS(inabatt)), // Battery
            DEVICE_DT_GET(DT_ALIAS(ina3v3)),  // 3v3
            DEVICE_DT_GET(DT_ALIAS(ina5v0))   // 5v0
    };

    bool ina_device_found[3] = {false};
    power_module_telemetry_t sensor_telemetry = {0};

    if (!init_ina_task(sensors, ina_device_found)) {
        return;
    }

    while (true) {
        k_timer_status_sync(&ina_task_timer);

        l_update_sensors_safe(sensors, 3, ina_device_found);

        l_get_shunt_data_float(sensors[0], &sensor_telemetry.data_battery);
        l_get_shunt_data_float(sensors[1], &sensor_telemetry.data_3v3);
        l_get_shunt_data_float(sensors[2], &sensor_telemetry.data_5v0);

        if (k_msgq_put(&ina_telemetry_msgq, &sensor_telemetry, K_NO_WAIT)) {
            LOG_ERR("Failed to put data into INA219 processing queue");
        }
    }
}

static void adc_task(void *, void *, void *) {
    static const float adc_gain = 0.09f;
    static const float mv_to_v_multiplier = 0.001f;
    const struct adc_dt_spec vin_sense_adc = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

    float vin_adc_data_mv = 0;
    uint16_t temp_vin_adc_data = 0;

    struct adc_sequence vin_sense_sequence = {
            .buffer = &temp_vin_adc_data,
            .buffer_size = sizeof(temp_vin_adc_data),
    };

    if (!init_adc_task(&vin_sense_adc, &vin_sense_sequence)) {
        return;
    }

    while (true) {
        k_timer_status_sync(&adc_task_timer);

        if (0 <= l_read_adc_mv(&vin_sense_adc, &vin_sense_sequence, (int32_t *) &vin_adc_data_mv)) {
            LOG_ERR("Failed to read ADC value from %d", vin_sense_adc.channel_id);
        }

        float vin_adc_data_v = (vin_adc_data_mv * mv_to_v_multiplier) * adc_gain;
        k_msgq_put(&adc_telemetry_msgq, &vin_adc_data_v, K_NO_WAIT);
    }
}