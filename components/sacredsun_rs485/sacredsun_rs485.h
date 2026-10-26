#pragma once
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <cmath>
#include "esphome/core/helpers.h"

namespace esphome
{
  namespace sacredsun
  {

#ifndef MAX
#define MAX(n, m) (((n) < (m)) ? (m) : (n))
#endif
#ifndef MIN
#define MIN(n, m) (((n) < (m)) ? (n) : (m))
#endif
#ifndef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#endif

    class SacredSunSensor : public PollingComponent, public uart::UARTDevice
    {
    public:
      SacredSunSensor(uart::UARTComponent *parent, int send_interval = 4000, int pack_count = 2,
                      uint32_t offline_timeout = 60000)
          : PollingComponent(10), uart::UARTDevice(parent)
      {
        this->send_interval_ = MAX(1000, send_interval);
        this->pack_count_ = MIN(9, pack_count);
        this->offline_timeout_ms_ = offline_timeout;
        for (uint8_t i = 0; i < MAX_PACKS; i++)
        {
          this->online_state_[i] = -1;
          this->last_valid_ms_[i] = 0;
          this->online_sensor_[i] = nullptr;
        }
      }
      struct PackData
      {
        sensor::Sensor *soc = nullptr;
        sensor::Sensor *voltage = nullptr;
        sensor::Sensor *current = nullptr;
        sensor::Sensor *soh = nullptr;
        sensor::Sensor *nominal_cap = nullptr;
        sensor::Sensor *remain_cap = nullptr;
        sensor::Sensor *cycles = nullptr;
        sensor::Sensor *remain_kwh = nullptr;
      };
      const std::string commands[9] = {
          "~22014A42E00201FD28\r",
          "~22024A42E00201FD27\r",
          "~22034A42E00201FD26\r",
          "~22044A42E00201FD25\r",
          "~22054A42E00201FD24\r",
          "~22064A42E00201FD23\r",
          "~22074A42E00201FD22\r",
          "~22084A42E00201FD21\r",
          "~22094A42E00201FD20\r"};
      static const uint8_t MAX_PACKS = 9;
      PackData pack[MAX_PACKS];
      PackData pack_all;
      // Buffer to store raw values for all packs (for avg/sum calculations)
      struct PackRawValues
      {
        float soc = 0.0f;
        float voltage = 0.0f;
        float current = 0.0f;
        float soh = 0.0f;
        float nominal_cap = 0.0f;
        float remain_cap = 0.0f;
        float cycles = 0.0f;
        float remain_kwh = 0.0f;
      };
      PackRawValues pack_raw[MAX_PACKS];
      uint8_t pack_count_ = 1;
      uint8_t current_pack = 0;
      unsigned long last_send_time_ = 0;
      // Offline detection: -1 = unknown (nothing decided yet), 0 = offline, 1 = online
      int8_t online_state_[MAX_PACKS];
      unsigned long last_valid_ms_[MAX_PACKS];
      binary_sensor::BinarySensor *online_sensor_[MAX_PACKS];
      void set_online_sensor(int8_t pack_idx, binary_sensor::BinarySensor *sens)
      {
        if (pack_idx >= 0 && pack_idx < MAX_PACKS)
          online_sensor_[pack_idx] = sens;
      }
      void add_sensor(std::string type, int8_t pack_idx, sensor::Sensor *sensor)
      {
        if (pack_idx >= MAX_PACKS)
          return;
        if (pack_idx >= 0)
        {
          if (type == "soc")
            pack[pack_idx].soc = sensor;
          else if (type == "voltage")
            pack[pack_idx].voltage = sensor;
          else if (type == "current")
            pack[pack_idx].current = sensor;
          else if (type == "soh")
            pack[pack_idx].soh = sensor;
          else if (type == "nominal_cap")
            pack[pack_idx].nominal_cap = sensor;
          else if (type == "remaining_cap")
            pack[pack_idx].remain_cap = sensor;
          else if (type == "cycles")
            pack[pack_idx].cycles = sensor;
          else if (type == "remaining_kwh")
            pack[pack_idx].remain_kwh = sensor;
        }
        // average for all:
        if (pack_idx == -1)
        {
          if (type == "soc")
            pack_all.soc = sensor;
          else if (type == "voltage")
            pack_all.voltage = sensor;
          else if (type == "current")
            pack_all.current = sensor;
          else if (type == "soh")
            pack_all.soh = sensor;
          else if (type == "nominal_cap")
            pack_all.nominal_cap = sensor;
          else if (type == "remaining_cap")
            pack_all.remain_cap = sensor;
          else if (type == "cycles")
            pack_all.cycles = sensor;
          else if (type == "remaining_kwh")
            pack_all.remain_kwh = sensor;
        }
      }
      uint8_t ascii_char_to_number(char c)
      {
        if (c >= '0' && c <= '9')
          return c - '0';
        if (c >= 'A' && c <= 'F')
          return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
          return c - 'a' + 10;
        return 0;
      }
      uint8_t ascii_to_byte(const char *str)
      {
        uint8_t h = ascii_char_to_number(str[0]);
        uint8_t l = ascii_char_to_number(str[1]);
        return (h << 4) + l;
      }
      int16_t ascii_to_integer(const char *str)
      {
        int16_t a = ascii_char_to_number(str[0]);
        int16_t b = ascii_char_to_number(str[1]);
        int16_t c = ascii_char_to_number(str[2]);
        int16_t d = ascii_char_to_number(str[3]);
        return (a << 12) + (b << 8) + (c << 4) + d;
      }
      uint8_t calculate_checksum8(const char *frame, uint16_t max_len)
      {
        uint8_t sum = 0;
        for (int i = 0; frame[i] != '\0' && i < max_len; i++)
        {
          sum += frame[i];
        }
        return 0x100 - sum;
      }
      void send_command(uint8_t pack_id)
      {
        if (pack_id < 9)
        {
          this->write_str(commands[pack_id].c_str());
        }
      }

      float pack_all_sum(uint8_t count_max, const std::string &sensor_type)
      {
        if (count_max == 0)
          return 0.0f;
        float sum = 0.0f;
        for (uint8_t i = 0; i < count_max; i++)
        {
          if (online_state_[i] != 1)
            continue;
          if (sensor_type == "soc")
            sum += pack_raw[i].soc;
          else if (sensor_type == "voltage")
            sum += pack_raw[i].voltage;
          else if (sensor_type == "current")
            sum += pack_raw[i].current;
          else if (sensor_type == "soh")
            sum += pack_raw[i].soh;
          else if (sensor_type == "nominal_cap")
            sum += pack_raw[i].nominal_cap;
          else if (sensor_type == "remaining_cap")
            sum += pack_raw[i].remain_cap;
          else if (sensor_type == "cycles")
            sum += pack_raw[i].cycles;
          else if (sensor_type == "remaining_kwh")
            sum += pack_raw[i].remain_kwh;
        }
        return sum;
      }

      float pack_all_average(uint8_t count_max, const std::string &sensor_type)
      {
        if (count_max == 0)
          return 0.0f;
        uint8_t valid_count = 0;
        for (uint8_t i = 0; i < count_max; i++)
        {
          if (online_state_[i] == 1)
            valid_count++;
        }
        if (valid_count == 0)
          return NAN;
        return pack_all_sum(count_max, sensor_type) / valid_count;
      }

      void setup() override
      {
        ESP_LOGD("sacredsun", "Setup called, pack_count=%d", pack_count_);
        unsigned long now = millis();
        for (uint8_t i = 0; i < MAX_PACKS; i++)
          last_valid_ms_[i] = now; // boot counts as "last seen" so the timeout starts now
        if (offline_timeout_ms_ > 0 && offline_timeout_ms_ < (unsigned long) send_interval_ * pack_count_ * 2)
          ESP_LOGW("sacredsun", "offline_timeout (%lu ms) is shorter than 2 full polling rounds (%lu ms), packs may flap",
                   (unsigned long) offline_timeout_ms_, (unsigned long) send_interval_ * pack_count_ * 2);
      }

      void publish_nan_for_pack(uint8_t i)
      {
        if (pack[i].soc) pack[i].soc->publish_state(NAN);
        if (pack[i].voltage) pack[i].voltage->publish_state(NAN);
        if (pack[i].current) pack[i].current->publish_state(NAN);
        if (pack[i].soh) pack[i].soh->publish_state(NAN);
        if (pack[i].nominal_cap) pack[i].nominal_cap->publish_state(NAN);
        if (pack[i].remain_cap) pack[i].remain_cap->publish_state(NAN);
        if (pack[i].cycles) pack[i].cycles->publish_state(NAN);
        if (pack[i].remain_kwh) pack[i].remain_kwh->publish_state(NAN);
      }

      void publish_aggregates()
      {
        if (pack_all.soc)
          pack_all.soc->publish_state(pack_all_average(pack_count_, "soc"));
        if (pack_all.voltage)
          pack_all.voltage->publish_state(pack_all_average(pack_count_, "voltage"));
        if (pack_all.current)
          pack_all.current->publish_state(pack_all_online_sum_or_nan("current"));
        if (pack_all.soh)
          pack_all.soh->publish_state(pack_all_average(pack_count_, "soh"));
        if (pack_all.nominal_cap)
          pack_all.nominal_cap->publish_state(pack_all_online_sum_or_nan("nominal_cap"));
        if (pack_all.remain_cap)
          pack_all.remain_cap->publish_state(pack_all_online_sum_or_nan("remaining_cap"));
        if (pack_all.cycles)
          pack_all.cycles->publish_state(pack_all_average(pack_count_, "cycles"));
        if (pack_all.remain_kwh)
          pack_all.remain_kwh->publish_state(pack_all_online_sum_or_nan("remaining_kwh"));
      }

      // Sum over online packs, NaN if no pack is online
      float pack_all_online_sum_or_nan(const std::string &sensor_type)
      {
        for (uint8_t i = 0; i < pack_count_; i++)
        {
          if (online_state_[i] == 1)
            return pack_all_sum(pack_count_, sensor_type);
        }
        return NAN;
      }

      void set_pack_online(uint8_t i, bool online)
      {
        if (online_state_[i] == (online ? 1 : 0))
          return;
        online_state_[i] = online ? 1 : 0;
        if (online)
        {
          ESP_LOGI("sacredsun", "Pack %d is online", i + 1);
        }
        else
        {
          ESP_LOGW("sacredsun", "Pack %d offline: no valid frame for %lu ms", i + 1, (unsigned long) offline_timeout_ms_);
          pack_raw[i] = PackRawValues();
          publish_nan_for_pack(i);
        }
        if (online_sensor_[i])
          online_sensor_[i]->publish_state(online);
        if (!online)
          publish_aggregates();
      }

      void check_offline_timeouts()
      {
        if (offline_timeout_ms_ == 0)
          return;
        unsigned long now = millis();
        for (uint8_t i = 0; i < pack_count_; i++)
        {
          if (online_state_[i] != 0 && (now - last_valid_ms_[i]) >= offline_timeout_ms_)
            set_pack_online(i, false);
        }
      }

      void update() override
      {
        const int resp_len = 211;
        const int buf_len = resp_len + 1;
        static uint8_t buf[buf_len];
        static size_t rec = 0;
        static bool is_wait_resp = false;

        check_offline_timeouts();

        if (millis() - last_send_time_ >= send_interval_)
        {
          current_pack = (current_pack + 1) % pack_count_;
          while (this->available())
          {
            this->read_byte(&buf[0]);
          }
          memset(buf, 0, buf_len);
          send_command(current_pack);
          ESP_LOGD("sacredsun", "Sent command for pack %d", current_pack);
          last_send_time_ = millis();
          rec = 0;
          is_wait_resp = true;
        }

        if (is_wait_resp && this->available() >= 1 && rec <= resp_len &&
            (millis() - last_send_time_) < send_interval_)
        {
          size_t to_read = MIN(this->available(), resp_len - rec);
          if (this->read_array(&buf[rec], to_read))
          {
            rec += to_read;
            ESP_LOGVV("sacredsun", "Received %d bytes, total rec=%d", to_read, rec);
          }

          if (rec >= resp_len && buf[0] == '~' && buf[1] == '2' && buf[2] == '2' && buf[207] == 'D')
          {
            is_wait_resp = false;
            uint8_t calc_chk = calculate_checksum8((const char *)&buf[1], 206);
            uint8_t wanted_chk = ascii_to_byte((const char *)&buf[209]);
            ESP_LOGD("sacredsun", "Frame detected. Calculated checksum: %02X, wanted: %02X", calc_chk, wanted_chk);
            if (calc_chk == wanted_chk)
            {
              last_valid_ms_[current_pack] = millis();
              set_pack_online(current_pack, true);
              // Store values for all packs
              float soc_val = ascii_to_integer((const char *)&buf[15]);
              float voltage_val = ascii_to_integer((const char *)&buf[19]);
              float current_val = ascii_to_integer((const char *)&buf[115]);
              float soh_val = ascii_to_integer((const char *)&buf[123]);
              float nominal_cap_val = ascii_to_integer((const char *)&buf[129]);
              //float remain_cap_val = ascii_to_integer((const char *)&buf[133]);
              float cycles_val = ascii_to_integer((const char *)&buf[137]);
              float remain_cap_val = nominal_cap_val * soc_val / 100;
              float remain_kwh_val = remain_cap_val * voltage_val * 0.001;

              // Store raw values in buffer for all packs (for avg/sum calculations)
              pack_raw[current_pack].soc = soc_val;
              pack_raw[current_pack].voltage = voltage_val;
              pack_raw[current_pack].current = current_val;
              pack_raw[current_pack].soh = soh_val;
              pack_raw[current_pack].nominal_cap = nominal_cap_val;
              pack_raw[current_pack].remain_cap = remain_cap_val;
              pack_raw[current_pack].cycles = cycles_val;
              pack_raw[current_pack].remain_kwh = remain_kwh_val;

              // Publish individual pack values if configured
              if (pack[current_pack].soc)
                pack[current_pack].soc->publish_state(soc_val);
              if (pack[current_pack].voltage)
                pack[current_pack].voltage->publish_state(voltage_val);
              if (pack[current_pack].current)
                pack[current_pack].current->publish_state(current_val);
              if (pack[current_pack].soh)
                pack[current_pack].soh->publish_state(soh_val);
              if (pack[current_pack].nominal_cap)
                pack[current_pack].nominal_cap->publish_state(nominal_cap_val);
              if (pack[current_pack].remain_cap)
                pack[current_pack].remain_cap->publish_state(remain_cap_val);
              if (pack[current_pack].cycles)
                pack[current_pack].cycles->publish_state(cycles_val);
              if (pack[current_pack].remain_kwh)
                pack[current_pack].remain_kwh->publish_state(remain_kwh_val);

              // Publish averages/sums for all online packs
              publish_aggregates();
            }
            else
            {
              ESP_LOGW("sacredsun", "Checksum mismatch! calc=%02X, wanted=%02X", calc_chk, wanted_chk);
            }
          }
          else if (rec >= resp_len)
          {
            ESP_LOGW("sacredsun", "Frame validation failed: rec=%d, buf[0]=%c, buf[1]=%c, buf[2]=%c, buf[207]=%c", rec, buf[0], buf[1], buf[2], buf[207]);
            is_wait_resp = false;
          }
        }
      }

    protected:
      unsigned long send_interval_;
      uint32_t offline_timeout_ms_ = 60000;
    };
  }
}