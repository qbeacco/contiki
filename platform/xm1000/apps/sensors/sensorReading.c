#include "contiki.h"
#include "dev/battery-sensor.h"
#include "lib/sensors.h"
#include "dev/light-sensor.h"
#include "dev/sht11-sensor.h"
#include <stdio.h> 

unsigned short d1(float f) // digits before decimal point
{
  return((unsigned short)f);
}
unsigned short d2(float f) // digits after decimal point
{
  return(1000*(f-d1(f)));
}

PROCESS(sensor_reading_process, "Sensor reading process");
AUTOSTART_PROCESSES(&sensor_reading_process);

PROCESS_THREAD(sensor_reading_process, ev, data)
{
  static struct etimer timer;
  PROCESS_BEGIN();
  etimer_set(&timer, CLOCK_CONF_SECOND);
  SENSORS_ACTIVATE(light_sensor);  
  SENSORS_ACTIVATE(sht11_sensor);
  SENSORS_ACTIVATE(battery_sensor);
  while(1) 
  {
    PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

    float temp = 0.01*sht11_sensor.value(SHT11_SENSOR_TEMP)-39.6;
    float rh = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

    float V_sensor = 1.5 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC)/4096;
    float I = V_sensor/100000;
    float light_lx = 0.625*1e6*I*1000;

    // Display every 1 second
    if(clock_seconds() % 1 == 0)
    {
         printf("%lu seconds %u%% humidity\n",clock_seconds(),(unsigned) (-4 + 0.0405*rh - 2.8e-6*(rh*rh)));
	
    }

    // Display every 2 seconds
    if(clock_seconds() % 2 == 0)
    {
          printf("%lu seconds %u.%03u Celsius degrees\n", clock_seconds(),d1(temp), d2(temp));
	  printf("%lu seconds %u.%03u lux\n", clock_seconds(), d1(light_lx), d2(light_lx));
    }   

    etimer_reset(&timer);
  }
  PROCESS_END();
}
