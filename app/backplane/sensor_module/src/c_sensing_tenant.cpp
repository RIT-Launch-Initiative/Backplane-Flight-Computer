#include "c_sensing_tenant.h"
#include "c_sensor_module.h"

#include <f_core/device/sensor/c_accelerometer.h>
#include <f_core/device/sensor/c_barometer.h>
#include <f_core/device/sensor/c_gyroscope.h>
#include <f_core/device/sensor/c_magnetometer.h>
#include <f_core/device/sensor/c_temperature_sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(CSensingTenant);

CSensingTenant::CSensingTenant(const char* name,
                               CMessagePort<NTypes::SensorData>& dataToBroadcast,
                               CMessagePort<NTypes::SensorData>& dataToLog)
    : CTenant(name),
      dataToBroadcast(dataToBroadcast),
      dataToLog(dataToLog),
      imuAccelerometer(*DEVICE_DT_GET(DT_ALIAS(imu))),
      imuGyroscope(*DEVICE_DT_GET(DT_ALIAS(imu))),
      primaryBarometer(*DEVICE_DT_GET(DT_ALIAS(primary_barometer))),
      secondaryBarometer(*DEVICE_DT_GET(DT_ALIAS(secondary_barometer))),
      accelerometer(*DEVICE_DT_GET(DT_ALIAS(accelerometer))),
      thermometer(*DEVICE_DT_GET(DT_ALIAS(thermometer))),
#ifndef CONFIG_ARCH_POSIX
      magnetometer(*DEVICE_DT_GET(DT_ALIAS(magnetometer))),
#endif
      sensors{
          &imuAccelerometer,
          &imuGyroscope,
          &primaryBarometer,
          &secondaryBarometer,
          &accelerometer,
          &thermometer
#ifndef CONFIG_ARCH_POSIX
          , &magnetometer
#endif
      } {}

void CSensingTenant::Startup() {
#ifndef CONFIG_ARCH_POSIX
    const sensor_value imuOdr{
        .val1 = 104,
        .val2 = 0
    };

    if (imuAccelerometer.Configure(SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &imuOdr)) {
        LOG_WRN("IMU Accelerometer ODR configuration failed. IMU accelerations will report 0.");
    }

    if (imuGyroscope.Configure(SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &imuOdr)) {
        LOG_WRN("IMU Gyroscope ODR configuration failed. IMU gyroscope values will report 0.");
    }

#endif
}

void CSensingTenant::PostStartup() {}

void CSensingTenant::Run() {
    NTypes::SensorData data{};
    while (true) {
        for (auto sensor : sensors) {
            if (sensor) {
                sensor->UpdateSensorValue();
            }
        }

        data.Acceleration.X = accelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_X);
        data.Acceleration.Y = accelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_Y);
        data.Acceleration.Z = accelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_Z);

        data.ImuAcceleration.X = imuAccelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_X);
        data.ImuAcceleration.Y = imuAccelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_Y);
        data.ImuAcceleration.Z = imuAccelerometer.GetSensorValueFloat(SENSOR_CHAN_ACCEL_Z);

        data.ImuGyroscope.X = imuGyroscope.GetSensorValueFloat(SENSOR_CHAN_GYRO_X);
        data.ImuGyroscope.Y = imuGyroscope.GetSensorValueFloat(SENSOR_CHAN_GYRO_Y);
        data.ImuGyroscope.Z = imuGyroscope.GetSensorValueFloat(SENSOR_CHAN_GYRO_Z);

#ifndef CONFIG_ARCH_POSIX
        data.Magnetometer.X = magnetometer.GetSensorValueFloat(SENSOR_CHAN_MAGN_X);
        data.Magnetometer.Y = magnetometer.GetSensorValueFloat(SENSOR_CHAN_MAGN_Y);
        data.Magnetometer.Z = magnetometer.GetSensorValueFloat(SENSOR_CHAN_MAGN_Z);
#endif

        data.PrimaryBarometer.Pressure = primaryBarometer.GetSensorValueFloat(SENSOR_CHAN_PRESS);
        data.PrimaryBarometer.Temperature = primaryBarometer.GetSensorValueFloat(SENSOR_CHAN_AMBIENT_TEMP);

        data.SecondaryBarometer.Pressure = secondaryBarometer.GetSensorValueFloat(SENSOR_CHAN_PRESS);
        data.SecondaryBarometer.Temperature = secondaryBarometer.GetSensorValueFloat(SENSOR_CHAN_AMBIENT_TEMP);

        data.Temperature = thermometer.GetSensorValueFloat(SENSOR_CHAN_AMBIENT_TEMP);

        dataToBroadcast.Send(data, K_MSEC(5));
        dataToLog.Send(data, K_MSEC(5));

        k_msleep(100);
    }
}
