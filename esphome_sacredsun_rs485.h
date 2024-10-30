#include "esphome.h"

#ifndef MAX
#define MAX(n, m) (((n) < (m)) ? (m) : (n))
#endif

#ifndef MIN
#define MIN(n, m) (((n) < (m)) ? (n) : (m))
#endif

#ifndef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#endif

class SacredSunSensor : public PollingComponent, public Sensor
{
public:
   SacredSunSensor(UARTComponent *_parentUart, int _sendInterval = 1000, int _packCount = 1) : PollingComponent(10)
   {
      this->parentUart = _parentUart;
      this->sendInterval = MAX(1000, _sendInterval);
      this->multiPackLen = MIN(multiPackMaxLen, _packCount);
   }

   typedef struct
   {
      Sensor *soc = new Sensor();
      Sensor *voltage = new Sensor();
      Sensor *celCnt = new Sensor();
      Sensor *vol[15] = {nullptr};
      Sensor *tempBms[3] = {nullptr};
      Sensor *tempCnt = new Sensor();
      Sensor *tempBat[4] = {nullptr};
      Sensor *current = new Sensor();
      Sensor *soh = new Sensor();
      Sensor *nominalCap = new Sensor();
      Sensor *remainCap = new Sensor();
      Sensor *cycles = new Sensor();
   } pack_t;

   const String commands[9] = {"~22014A42E00201FD28\r", // address 1
                               "~22024A42E00201FD27\r", // address 2
                               "~22034A42E00201FD26\r", // etc
                               "~22044A42E00201FD25\r",
                               "~22054A42E00201FD24\r",
                               "~22064A42E00201FD23\r",
                               "~22074A42E00201FD22\r",
                               "~22084A42E00201FD21\r",
                               "~22094A42E00201FD20\r"};
   static const uint8_t multiPackMaxLen = sizeof(commands) / sizeof(commands[0]);
   pack_t pack[multiPackMaxLen];
   uint8_t multiPackLen = 1;
   uint8_t current_pack = 0;
   unsigned long lastSendTime = 0; // To keep track of the last send time

   // Helper function to convert ASCII characters to their hexadecimal equivalent
   uint8_t asciiCharToNumber(char c)
   {
      if (c >= '0' && c <= '9')
      {
         return c - '0'; // Convert ASCII '0'-'9' to hex
      }
      else if (c >= 'A' && c <= 'F')
      {
         return c - 'A' + 10; // Convert ASCII 'A'-'F' to hex
      }
      else if (c >= 'a' && c <= 'f')
      {
         return c - 'a' + 10; // Convert ASCII 'a'-'f' to hex
      }
      return 0; // Return 0 for any invalid character
   }

   uint8_t asciiToByte(const char *str)
   {
      int8_t H = asciiCharToNumber(str[0]);
      int8_t L = asciiCharToNumber(str[1]);
      return ((H) << 4) + ((L));
   }

   int16_t asciiToInteger(const char *str)
   {
      int16_t a = asciiCharToNumber(str[0]);
      int16_t b = asciiCharToNumber(str[1]);
      int16_t c = asciiCharToNumber(str[2]);
      int16_t d = asciiCharToNumber(str[3]);
      return ((a << 12) + (b << 8) + (c << 4) + (d));
   }

   // calculate CheckSum8 (2's complement)
   // CheckSum8 is 0x100 - sum of bytes (mod 256)
   uint8_t calculateChecksum8(const char *rxFrame, uint16_t maxLen)
   {
      uint8_t sum = 0;
      int i = 0;
      while (rxFrame[i] != '\0' && i < maxLen)
      {
         sum += rxFrame[i];
         i++;
      }
      // CheckSum8 is 0x100 - sum of bytes (mod 256)
      uint8_t calcChk = 0x100 - sum;
      return calcChk;
   }

   void sendCommand(const int _current_pack)
   {
      ESP_LOGD("custom", "%s(%d)", __func__, _current_pack);
      parentUart->write_str(commands[_current_pack].c_str());
   }

   size_t mReadBytes(uint8_t *pbuf, int maxLen)
   {
      // bool 	read_array (uint8_t *data, size_t len)
      int readLen = MIN(parentUart->available(), maxLen);
      parentUart->read_array(pbuf, readLen);
      return readLen;
   }

   void setup() override
   {
      // This will be called once to set up the component
      for (int pk = 0; pk < multiPackMaxLen; pk++)
      {
         for (int i = 0; i < sizeof(pack[pk].vol) / sizeof(pack[pk].vol[0]); i++)
         {
            pack[pk].vol[i] = new Sensor();
         }
         for (int i = 0; i < sizeof(pack[pk].tempBms) / sizeof(pack[pk].tempBms[0]); i++)
         {
            pack[pk].tempBms[i] = new Sensor();
         }
         for (int i = 0; i < sizeof(pack[pk].tempBat) / sizeof(pack[pk].tempBat[0]); i++)
         {
            pack[pk].tempBat[i] = new Sensor();
         }
      }
   }

   void loop() override
   {
      // esphome use update() instead
   }

   // update interval set in 'PollingComponent(xxxx)'
   void update() override
   {
      const int respLen = 211;
      const int bufLen = respLen + 1;
      static uint8_t buf[bufLen];
      static size_t rec = 0;
      static bool isWaitResp = false;

      // Send the command every 1 second
      if (millis() - lastSendTime >= sendInterval)
      {
         current_pack = (current_pack + 1) % multiPackLen;
         while (parentUart->available()) // flush Rx
         {
            this->mReadBytes(&buf[0], respLen);
         }
         memset(buf, 0, bufLen);
         this->sendCommand(current_pack);
         lastSendTime = millis();
         rec = 0;
         isWaitResp = true;
      }

      if (isWaitResp && parentUart->available() >= 1 && rec <= respLen && (millis() - lastSendTime) < sendInterval)
      {
         ESP_LOGD("custom", "read  available %d", parentUart->available());
         rec += this->mReadBytes(&buf[rec], respLen - rec);
         ESP_LOGD("custom", "rec %d", rec);

         // check frame is valid
         if (rec >= respLen && buf[0] == '~' && buf[1] == '2' && buf[2] == '2' && buf[207] == 'D')
         {
            isWaitResp = false;
            const uint8_t calcChk = calculateChecksum8((const char *)&buf[1], 206);
            const uint8_t wantedChk = asciiToByte((const char *)&buf[209]);
            if (calcChk == wantedChk)
            {
               ESP_LOGD("custom", "%s", buf);
               uint8_t address = asciiToByte((const char *)&buf[3]);
               if (current_pack == address - 1)
               {
                  this->pack[current_pack].soc->publish_state(asciiToInteger((const char *)&buf[15]));
                  this->pack[current_pack].voltage->publish_state(asciiToInteger((const char *)&buf[19]));
                  this->pack[current_pack].celCnt->publish_state(asciiToByte((const char *)&buf[23]));
                  for (size_t i = 0; i < 15; i++)
                  {
                     this->pack[current_pack].vol[i]->publish_state(asciiToInteger((const char *)&buf[25 + i * 4]));
                  }
                  for (size_t i = 0; i < 3; i++)
                  {
                     this->pack[current_pack].tempBms[i]->publish_state(asciiToInteger((const char *)&buf[85 + i * 4]));
                  }
                  this->pack[current_pack].tempCnt->publish_state(asciiToByte((const char *)&buf[97]));
                  for (size_t i = 0; i < 4; i++)
                  {
                     this->pack[current_pack].tempBat[i]->publish_state(asciiToInteger((const char *)&buf[99 + i * 4]));
                  }
                  this->pack[current_pack].current->publish_state(asciiToInteger((const char *)&buf[115]));
                  this->pack[current_pack].soh->publish_state(asciiToInteger((const char *)&buf[123]));
                  this->pack[current_pack].nominalCap->publish_state(asciiToInteger((const char *)&buf[129]));
                  this->pack[current_pack].remainCap->publish_state(asciiToInteger((const char *)&buf[133]));
                  this->pack[current_pack].cycles->publish_state(asciiToInteger((const char *)&buf[137]));
               }
            }
            else
            {
               ESP_LOGD("custom", "%s, chk=%x != %x FAILED", buf, calcChk, wantedChk);
            }
         }
      }
   }

protected:
   UARTComponent *parentUart;
   unsigned long sendInterval; // interval
};
