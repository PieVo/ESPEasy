//#######################################################################################################
//#################################### Plugin 053: Plantower PMSx003 ####################################
//#######################################################################################################
//
// http://www.aqmd.gov/docs/default-source/aq-spec/resources-page/plantower-pms5003-manual_v2-3.pdf?sfvrsn=2
//
// The PMSx003 are particle sensors. Particles are measured by blowing air though the enclosue and,
// togther with a laser, count the amount of particles. These sensors have an integrated microcontroller
// that counts particles and transmits measurement data over the serial connection.

#ifdef PLUGIN_BUILD_TESTING

#include <SoftwareSerial.h>

#define PLUGIN_053
#define PLUGIN_ID_053 53
#define PLUGIN_NAME_053 "Particle Sensor - PMSx003"
#define PLUGIN_VALUENAME1_053 "pm1.0"
#define PLUGIN_VALUENAME2_053 "pm2.5"
#define PLUGIN_VALUENAME3_053 "pm10"
#define PMSx003_SIG1 0X42
#define PMSx003_SIG2 0X4d
#define PMSx003_SIZE 32

SoftwareSerial *swSerial = NULL;
boolean Plugin_053_init = false;
byte timer = 0;
boolean values_received = false;

void SerialRead16(uint16_t* value, uint16_t* checksum)
{
  uint8_t data_high, data_low;

  // If swSerial is initialized, we are using soft serial
  if (swSerial != NULL)
  {
    while(swSerial.available() < 2) {};
    data_high = swSerial.read();
    data_low = swSerial.read();
  }
  else
  {
    while(Serial.available() < 2) {};
    data_high = Serial.read();
    data_low = Serial.read();
  }

  *value = data_low;
  *value |= (data_high << 8);

  if (checksum != NULL)
  {
    *checksum += data_high;
    *checksum += data_low;
  }
// Low-level logging to see data from sensor
#if 0
  String log = F("PMSx003 : byte high=0x");
  log += String(data_high,HEX);
  log += F(" byte low=0x");
  log += String(data_low,HEX);
  log += F(" result=0x");
  log += String(*value,HEX);
  addLog(LOG_LEVEL_INFO, log);
#endif
}

boolean PacketAvailable(void)
{
  boolean ret = false;

  if (swSerial != NULL)
  { 
    // First check if there is a complete packet (needed?)
    if (swSerial.available >= PMSx003_SIZE)
    {
      // Search through the buffer to find header (buffer may be out of sync)
      while (swSerial.available() > 1)
      {
        if (swSerial.read() == PMSx003_SIG1 && swSerial.read() == PMSx003_SIG2)
          ret = true;
      }
    }
  }
  else
  {
    if (Serial.available >= PMSx003_SIZE && 
        Serial.read() == PMSx003_SIG1 && Serial.read() == PMSx003_SIG2)
    {
      ret = true;
    }
  }
  return ret;
}

boolean Plugin_053(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_053;
        Device[deviceCount].Type = DEVICE_TYPE_DUAL;
        Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].ValueCount = 3;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        success = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_053);
        success = true;
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_053));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_053));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_053));
        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        String Log;
        int rxPin = Settings.TaskDevicePin1[event->TaskIndex];
        int txPin = Settings.TaskDevicePin2[event->TaskIndex];
        int resetPin = Settings.TaskDevicePin3[event->TaskIndex];
        log = F("PMSx003 : config ") + rxPin + txPin + resetPin;
        addLog(LOG_LEVEL_INFO, log);

        // Hardware serial is RX on 3 and TX on 1
        if (rxPin == 3 && txPin == 1)
        {
          log = F("PMSx003 : using hardware serial"); 
          addLog(LOG_LEVEL_INFO, log);
          Serial.begin(9600);
        {
        else
        {
          log = F("PMSx003: using software serial");
          addLog(LOG_LEVEL_INFO, log);
          swSerial = new SoftwareSerial(rxPin, txPin);
          swSerial.begin(9600);
        }
        
        if (resetPin)
        {
          // Toggle 'reset' to assure we start reading header
          log = F("PMSx003: resetting module");
          addLog(LOG_LEVEL_INFO, log);
          pinMode(resetPin, OUTPUT);
          digitalWrite(resetPin, LOW);
          delay(250);
          digitalWrite(resetPin, HIGH);
          pinMode(resetPin, INPUT_PULLUP);
        }
        
        Plugin_053_init = true;
        success = true;
        break;
      }

    case PLUGIN_READ:
      {
        if (Plugin_053_init)
        {
          uint16_t checksum = 0, checksum2 = 0;
          uint16_t framelength = 0;
          uint16_t data[13];
          byte data_low, data_high;
          int i = 0;
          String log;

          // Check if a packet is available in the UART buffer.
          if (PacketAvailable())
          {
            checksum += PMSx003_SIG1 + PMSx003_SIG2;
            SerialRead16(&framelength, &checksum);
            if (framelength != (PMSx003_SIZE - 4))
            {
              log = F("PMSx003 : invalid framelength - ");
              log+=framelength;
              addLog(LOG_LEVEL_INFO, log);
              break;
            }

            for (i = 0; i < 13; i++)
            {
              SerialRead16(&data[i], &checksum);
            }

            log = F("PMSx003 : pm1.0=");
            log += data[0];
            log += F(", pm2.5=");
            log += data[1];
            log += F(", pm10=");
            log += data[2];
            log += F(", pm1.0a=");
            log += data[3];
            log += F(", pm2.5a=");
            log += data[4];
            log += F(", pm10a=");
            log += data[5];
            addLog(LOG_LEVEL_INFO, log);

            log = F("PMSx003 : count/0.1L : 0.3um=");
            log += data[6];
            log += F(", 0.5um=");
            log += data[7];
            log += F(", 1.0um=");
            log += data[8];
            log += F(", 2.5um=");
            log += data[9];
            log += F(", 5.0um=");
            log += data[10];
            log += F(", 10um=");
            log += data[11];
            addLog(LOG_LEVEL_INFO, log);

            // Compare checksums
            SerialRead16(&checksum2, NULL);
            if (checksum == checksum2)
            {
              /* Data is checked and good, fill in output */
              UserVar[event->BaseVarIndex]     = data[3];
              UserVar[event->BaseVarIndex + 1] = data[4];
              UserVar[event->BaseVarIndex + 2] = data[5];
              success = true;
            }
          }
        }
        break;
      }
  }
  return success;
}
#endif // PLUGIN_BUILD_TESTING
