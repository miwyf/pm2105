/*********************************************************************
        MiWyf AIR For PM2105 V1.0
--------------------- 20230105 by MiWyf ------------------------------
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
--------------------------------------------------------------------

********************************************************************/
#include "esphome.h"
#include "Wire.h"

// For Debug
//#define PM2105i_DEBUG

static const char *TAGpm2105i = "pm2105i";

#define PM2105i_ADDRESS                    0x28

// Control modes
#define PM2105i_CTRL_CLOSE_MEASUREMENT                 0x1
#define PM2105i_CTRL_OPEN_SINGLE_MEASUREMENT           0x2
#define PM2105i_CTRL_SET_UP_CONTINUOUSLY_MEASUREMENT   0x3
#define PM2105i_CTRL_SET_UP_TIMING_MEASUREMENT         0x4
#define PM2105i_CTRL_SET_UP_DYNAMIC_MEASUREMENT        0x5
#define PM2105i_CTRL_SET_UP_CALIBRATION_COEFFICIENT    0x6
#define PM2105i_CTRL_SET_UP_WARM_MODE                  0x7

#define PM2105i_CONTROL_MODE               PM2105i_CTRL_SET_UP_CONTINUOUSLY_MEASUREMENT

#define PM2105i_MEASURING_TIME             180
#define PM2105i_CALIBRATION_COEFFICIENT    70
#define PM2105i_FRAME_HEADER               0x16

// Status
#define PM2105i_STATUS_CLOSE               0x1
#define PM2105i_STATUS_UNDER_MEASURING     0x2
#define PM2105i_STATUS_FAILED              0x7
#define PM2105i_STATUS_DATA_STABLE         0x80

// AQI
#define AQI_BASE_US             0
#define AQI_BASE_CN             1

#define AQI_DATA                0
#define AQI_LEVEL               1
#define AQI_MAIN_POLLUTANT      2

#define POLLUTANT_PM2_5         0
#define POLLUTANT_PM10          1

class PM2105Sensor : public PollingComponent, public I2CDevice, public Sensor{
public:
  Sensor *pm1p0_sensor = new Sensor();
  Sensor *pm2p5_sensor = new Sensor();
  Sensor *pm10_sensor = new Sensor();
  Sensor *AQI_s = new Sensor(); 
  Sensor *AQILevel_s = new Sensor();  
  Sensor *MainPollutedn_s = new Sensor();  

  PM2105Sensor(I2CBus *bus, uint8_t address, bool vref_default = true){
    set_i2c_address(address);
    set_i2c_bus(bus);
    this->vref_default_ = vref_default;
  }

  PM2105Sensor() : PollingComponent(15000) { }

  void setup() override{
    this->set_update_interval(15000);
    delay(1000);
    Wire.begin();
    command();
    #ifdef PM2105i_DEBUG
    ESP_LOGD(TAG, "Setting up PM2105 at %#02x ...", address_);
    #endif
  }

  void update() override {
    uint8_t ret = read();
    //ESP_LOGD(TAGpm2105i, "pm2105 update! ret=%d",ret);
    if (ret == 0) {
      pm1p0_sensor->publish_state(this->pm1p0_grimm);
      pm2p5_sensor->publish_state(this->pm2p5_grimm);
      pm10_sensor->publish_state(this->pm10_grimm);
      AQI_s->publish_state(this->AQI);
      AQILevel_s->publish_state(this->AQILevel);
      MainPollutedn_s->publish_state(this->MainPollutedn);

    }
    //delay(1000);
  }

  /**
  * Send command data
  */
  void command() {
    uint16_t data;
    this->_buffer[0] = PM2105i_FRAME_HEADER;
    this->_buffer[1] = 0x7; // frame length
    this->_buffer[2] = PM2105i_CONTROL_MODE;

    switch (PM2105i_CONTROL_MODE) {
      case PM2105i_CTRL_SET_UP_CONTINUOUSLY_MEASUREMENT:
        data = 0xFFFF;
        break;
      case PM2105i_CTRL_SET_UP_CALIBRATION_COEFFICIENT:
        data = PM2105i_CALIBRATION_COEFFICIENT;
        break;
      default:
        data = PM2105i_MEASURING_TIME;
        break;
    }

    this->_buffer[3] = data >> 8;
    this->_buffer[4] = data & 0xFF;
    this->_buffer[5] = 0; // Reserved

    // Calculate checksum
    this->_buffer[6] = this->_buffer[0];

    for (uint8_t i = 1; i < 6; i++) {
      this->_buffer[6] ^= this->_buffer[i];
    }

    Wire.beginTransmission(PM2105i_ADDRESS);
    Wire.write(this->_buffer, 7);
    Wire.endTransmission();
      #ifdef PM2105i_DEBUG
      ESP_LOGD(TAGpm2105i, "cmd %#02x ...", address_);
      #endif
  }

  /**
  * Read PM2105 value
  * @return {@code 0} Reading PM2105 value succeeded
  *         {@code 1} Buffer(index) is short
  *         {@code 2} Frame header is different
  *         {@code 3} Frame length is not 22
  *         {@code 4} Checksum is wrong
  */
  uint8_t read() {
    Wire.requestFrom(PM2105i_ADDRESS, 22);
    uint8_t idx = 0;

    while (Wire.available()) { // slave may send less than requested
      uint8_t b = Wire.read();
      _buffer[idx++] = b;
      if (idx == 22) break;
    }
    if (idx < 22) {
      #ifdef PM2105i_DEBUG
      ESP_LOGD(TAGpm2105i, "PM2105::read : buffer is short!");
      #endif
      return 1;
    }

    // Check frame header
    if (_buffer[0] != PM2105i_FRAME_HEADER) {
  #ifdef PM2105i_DEBUG
      ESP_LOGD(TAGpm2105i, "PM2105::read : frame header is different %#02x ...", _buffer[0]);
  #endif
      return 2;
    }

    // Check frame length
    if (_buffer[1] != 22) { 
      #ifdef PM2105i_DEBUG
      ESP_LOGD(TAGpm2105i, "PM2105::read : frame length is not 22 %#02x ...", _buffer[1]);
      #endif
      return 3;
    }

    // Check checksum
    uint8_t check_code = _buffer[0];
    for (uint8_t i = 1; i < 21; i++) {
      check_code ^= _buffer[i];
      #ifdef PM2105i_DEBUG
      //ESP_LOGD(TAGpm2105i, "%#02x", _buffer[i]);
      #endif
    }

    if (_buffer[21] != check_code) {
      #ifdef PM2105i_DEBUG
      ESP_LOGD(TAGpm2105i, "PM2105::read failed : check code is different - _buffer[21] : %#02x, check_code : %#02x", _buffer[21], check_code);
      #endif
      return 4;
    }
    // Status
    this->status = _buffer[2];
    this->measuring_mode = (_buffer[9] << 8) + _buffer[10];
    this->calibration_coefficient = (_buffer[11] << 8) + _buffer[12];
    this->pm1p0_grimm = (_buffer[3] << 8) + _buffer[4];
    this->pm2p5_grimm = (_buffer[5] << 8) + _buffer[6];
    this->pm10_grimm = (_buffer[7] << 8) + _buffer[8];
    parseAQI();
    this->AQI = getAQI(this->vref_default_);
    this->MainPollutedn = getMainPolluted(this->vref_default_);
    this->MainPolluted = getMainPollu(this->vref_default_);
    this->AQILevel = getAQILevel(this->vref_default_);
    this->AQILevel_CN = level2cn(AQILevel);
    #ifdef PM2105i_DEBUG
    ESP_LOGD(TAGpm2105i, "status:%u measuring_mode:%u calibration_coefficient:%u", this->status, this->measuring_mode, this->calibration_coefficient);
    ESP_LOGD(TAGpm2105i, "PM1.0:%u PM2.5:%u PM10:%u, AQI=%u  MainPolluted=%s AQILevel=%u", this->pm1p0_grimm, this->pm2p5_grimm, this->pm10_grimm, this->AQI, this->MainPolluted, this->AQILevel);
    #endif
    return 0;
  }

  /**
  * level to cn
  */ 
  String level2cn(uint16_t level)
  { 
    switch(level){
      case 0:  
        return "优";
        break;
      case 1: 
        return "良";
      break;
      case 2:  
        return "轻度污染";
      break;
      case 3: 
        return "中度污染";
      break;
      case 4: 
        return "重度污染";
      break;
      case 5: 
        return "严重污染";
      break;
      default:
        return "优";
        break;
    }
  }
  /**
  * get AQI
  */ 
  uint16_t getAQI(uint8_t _base) {
      if (_base >= AQI_BASE_US && _base <= AQI_BASE_CN) {
        return AQIBUFFER[_base][AQI_DATA];
      }
      else {
        return AQIBUFFER[AQI_BASE_US][AQI_DATA];
      }
  }
  /**
  * get AQI level
  */ 
  uint8_t getAQILevel(uint8_t _base) {
      if (_base >= AQI_BASE_US && _base <= AQI_BASE_CN) {
        return AQIBUFFER[_base][AQI_LEVEL];
      }
      else {
        return AQIBUFFER[AQI_BASE_US][AQI_LEVEL];
      }
  }
  /**
  * get main pollu
  */ 
  String getMainPollu(uint8_t _base) {
      if (_base >= AQI_BASE_US && _base <= AQI_BASE_CN) {
        return AQIBUFFER[_base][AQI_MAIN_POLLUTANT] ? "PM10" : "PM2.5";
      }
      else {
        return AQIBUFFER[AQI_BASE_US][AQI_MAIN_POLLUTANT] ? "PM10" : "PM2.5";
      }
  }
  /**
  * 0: PM2.5, 1: PM10
  */ 
  uint8_t getMainPolluted(uint8_t _base) {
      if (_base >= AQI_BASE_US && _base <= AQI_BASE_CN) {
        return AQIBUFFER[_base][AQI_MAIN_POLLUTANT] ? 1 : 0;
      }
      else {
        return AQIBUFFER[AQI_BASE_US][AQI_MAIN_POLLUTANT] ? 1 : 0;
      }
  }
  /**
  * parse PM to AQI
  */ 
  void parseAQI() {
      uint16_t AQI25, AQI100, color;
      for (uint8_t Bnum = 0; Bnum < 2; Bnum++) {
      // uint8_t Bnum = 0;
          AQI25 = 0;
          AQI100 = 0;
          for (uint8_t Inum = 1; Inum < 8; Inum++) {
              if (pm2p5_grimm*10 <= AQIindex[Inum][0+Bnum]) {
                  // IOT_DEBUG_PRINT4(F("Inum: "), Inum, F("Bnum: "), Bnum);
                  // IOT_DEBUG_PRINT2(F("AQIindex[Inum][0+Bnum]: "), AQIindex[Inum][0+Bnum]);
                  AQI25 = ((AQIindex[Inum][4] - AQIindex[Inum-1][4])*(pm2p5_grimm*10 - AQIindex[Inum-1][0+Bnum]) / (AQIindex[Inum][0+Bnum] - AQIindex[Inum - 1][0+Bnum]) + AQIindex[Inum-1][4])/10;
                  color = AQIindex[Inum][5];
                  break;
              }
              if (Inum == 7) {
                  AQI25 = 500;
                  color = 5;
              }
          }
          for (uint8_t Inum = 1; Inum < 8; Inum++) {
              if (pm10_grimm*10 <= AQIindex[Inum][2+Bnum]) {
                  // IOT_DEBUG_PRINT4(F("Inum: "), Inum, F("Bnum: "), Bnum);
                  // IOT_DEBUG_PRINT2(F("AQIindex[Inum][0+Bnum]: "), AQIindex[Inum][0+Bnum]);
                  AQI100 = ((AQIindex[Inum][4] - AQIindex[Inum-1][4])*(pm10_grimm*10 - AQIindex[Inum-1][2+Bnum]) / (AQIindex[Inum][2+Bnum] - AQIindex[Inum - 1][2+Bnum]) + AQIindex[Inum-1][4])/10;
                  
                  // IOT_DEBUG_PRINT4(F("AQI25: "), AQI25, F("  AQI100: "), AQI100);
                  if(AQI25 >= AQI100) {
                      // return String(AQI25);
                      AQIBUFFER[Bnum][AQI_DATA] = AQI25;
                      AQIBUFFER[Bnum][AQI_LEVEL] = color;
                      AQIBUFFER[Bnum][AQI_MAIN_POLLUTANT] = POLLUTANT_PM2_5;
                      break;
                  }
                  else {
                      AQIBUFFER[Bnum][AQI_DATA] = AQI100;
                      color = AQIindex[Inum][5];
                      AQIBUFFER[Bnum][AQI_LEVEL] = color;
                      AQIBUFFER[Bnum][AQI_MAIN_POLLUTANT] = POLLUTANT_PM10;
                      break;
                      // return String(AQI100);
                  }
              }
              if (Inum == 7) {
                  AQIBUFFER[Bnum][0] = 500;
                  AQIBUFFER[Bnum][1] = 5;
              }
          }
      }
  }

protected:
  bool vref_default_{true};
  const uint16_t AQIindex[8][6] = {
      {0,     0,      0,      0,      0,      0},
      {120,   350,    540,    500,    500,    0},//35 50
      {354,   750,    1540,   1500,   1000,   1},//75 150
      {554,   1150,   2540,   2500,   1500,   2},//115 250
      {1504,  1500,   3540,   3500,   2000,   3},//150 350
      {2504,  2500,   4240,   4200,   3000,   4},//250 420
      {3504,  3500,   5040,   5000,   4000,   5},//350 500
      {5004,  5000,   6040,   6000,   5000,   5}//500 600C I
  };
  uint16_t    AQIBUFFER[2][3] = {{0,0,0},{0,0,0}};
  uint8_t     _buffer[32];
  uint8_t     status;
  uint16_t    measuring_mode;
  uint16_t    calibration_coefficient;
  uint16_t    pm1p0_grimm;
  uint16_t    pm2p5_grimm;
  uint16_t    pm10_grimm;
  uint16_t    AQI;
  uint8_t     AQILevel;
  uint8_t     MainPollutedn;
  String      MainPolluted;
  String      AQILevel_CN;

};
