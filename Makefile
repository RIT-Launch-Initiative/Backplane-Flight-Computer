all:
	west build

flash:
	west flash

nucleo:
	west build -b nucleo_f446re app -DSHIELD=launch_mikroe -p

power:
	west build -b power_module app_power_module -p auto 

radio:
	west build -b radio_module app_radio_module -p auto 

sensor:
	west build -b sensor_module app_sensor_module -p auto 

