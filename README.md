# PM2105/PM2005 For ESPHome
CUBIC PM2105/PM2005 I2C

| Name                 |  State   | Actions |
| :------------------- | :------: | :------ |
| PM2105 AQI           |    76    |         |
| PM2105 AQI_CN        |    良    |         |
| PM2105 MainPolluted  |  PM2.5   |         |
| PM2105 PM1.0 Sensor  | 50 ug/m3 |         |
| PM2105 PM10.0 Sensor | 61 ug/m3 |         |
| PM2105 PM2.5 Sensor  | 56 ug/m3 |         |

1.Copy pm2105i2c.h to ESPHome config includes/

2.Copy this code to your_esphome.yaml

```yaml
#esphome.yaml
esphome:
  includes:
    - includes/pm2105i2c.h
i2c:
- id: bus_a
  sda: 23
  scl: 22
  scan: True
sensor:
  ## true:AQI_BASE_CN false:AQI_BASE_US
  - platform: custom
    lambda: |-
      auto pm2105 = new PM2105Sensor(bus_a, 0x28, true);
      App.register_component(pm2105);
      return {pm2105->pm1p0_sensor, pm2105->pm2p5_sensor, pm2105->pm10_sensor, pm2105->AQI_s, pm2105->AQILevel_s, pm2105->MainPollutedn_s};
    sensors:
      - name: "${friendly_name} PM1.0 Sensor"
        unit_of_measurement: ug/m3
      - name: "${friendly_name} PM2.5 Sensor"
        id: pm25
        unit_of_measurement: ug/m3
      - name: "${friendly_name} PM10.0 Sensor"
        unit_of_measurement: ug/m3
      - name: "${friendly_name} AQI"
        id: aqi
      - id: aqilevel
        #name: "${friendly_name} AQILevel"
        on_value:
          then:
            - lambda: |-  
                if (x == 5) {
                  id(aqi_cn).publish_state("严重污染");
                }else if (x == 4) {
                  id(aqi_cn).publish_state("重度污染");
                }else if (x == 3) {
                  id(aqi_cn).publish_state("中度污染");
                }else if (x == 2) {
                  id(aqi_cn).publish_state("轻度污染");
                }else if (x == 1) {
                  id(aqi_cn).publish_state("良");
                }else if (x == 0) {
                  id(aqi_cn).publish_state("优");
                }
      - id: mainpollutedn
        #name: "${friendly_name} MainPollutedn"
        on_value:
          then:
            - lambda: |-
                if (x == 1) {
                  id(mainpolluted).publish_state("PM10");
                } else {
                  id(mainpolluted).publish_state("PM2.5");
                }  
text_sensor:
  - platform: template
    name: "${friendly_name} AQI_CN"
    id: aqi_cn
  - platform: template
    name: "${friendly_name} MainPolluted"
    id: mainpolluted   
```

