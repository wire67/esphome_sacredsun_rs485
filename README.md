# SacredSun RS-485 for ESPHome

An external ESPHome component that polls SacredSun SCIFP48100 LiFePO4 battery packs over RS-485 and publishes their telemetry as Home Assistant entities. It supports up to nine addressed packs, per-pack health monitoring, combined multi-pack sensors, and optional cell-voltage-based SOC correction.

The protocol has been developed against the SacredSun SCIFP48100 with a Tian BMS. Compatibility with other models or firmware variants has not been verified.

## Features

- SOC, voltage, current, state of health, nominal and remaining capacity, cycle count, and estimated remaining energy.
- Lowest and highest cell voltage, plus optional corrected SOC.
- Per-pack online status and stale-value invalidation after a configurable timeout.
- Combined sensors across online packs. SOC, voltage, SOH, cycle count, and corrected SOC are averaged; current, capacity, and energy are summed; cell sensors report the lowest or highest cell across online packs.
- Configurable polling interval, pack count, and SOC correction thresholds.

## Installation and configuration

Add the component source to your ESPHome device configuration:

```yaml
external_components:
  - source: github://wire67/esphome_sacredsun_rs485@components
    refresh: 0s
```

Configure the UART and component. The component defaults to a 4-second polling interval, two packs, and a 60-second offline timeout. The `pack` index in ESPHome is zero-based (`0` is BMS address `1`).

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO22
  rx_pin: GPIO23
  baud_rate: 9600

sacredsun_rs485:
  uart_id: uart_bus
  send_interval: 4000
  pack_count: 2
  offline_timeout: 60s

sensor:
  - platform: sacredsun_rs485
    pack: 0
    sensors:
      - name: "Pack 1 SOC"
        type: soc
        unit_of_measurement: "%"
        filters:
          - multiply: 0.01
      - name: "Pack 1 Voltage"
        type: voltage
        unit_of_measurement: "V"
        filters:
          - multiply: 0.01

binary_sensor:
  - platform: sacredsun_rs485
    pack: 0
    name: "Pack 1 Online"
```

The complete two-pack example, including Home Assistant API, OTA, and primary sensors, is in [sample-esp-sacredsun.yaml](sample-esp-sacredsun.yaml).

### Supported sensor types

| Type | Raw unit | Notes |
| --- | --- | --- |
| `soc` | 0.01% | Apply `multiply: 0.01` for percent. |
| `voltage` | 0.01 V | Apply `multiply: 0.01` for volts. |
| `current` | 0.01 A | Apply `multiply: 0.01` for amperes. |
| `soh` | % | State of health. |
| `nominal_cap` | 0.01 Ah | Apply `multiply: 0.01` for amp-hours. |
| `remaining_cap` | 0.01 Ah | Apply `multiply: 0.01` for amp-hours. |
| `remaining_kwh` | 0.1 Wh | Apply `multiply: 0.0001` for kWh. |
| `cycles` | cycles | Cycle count. |
| `soc_corrected` | 0.01% | Apply `multiply: 0.01` for percent. |
| `cell_min`, `cell_max` | mV | Lowest and highest valid cell voltage in the pack. |

Set `pack: -1` on a sensor platform to create a combined sensor for configured packs. A `binary_sensor` requires an individual pack index from `0` to `8`.

## Hardware and bus settings

- ESP32 or ESP8266 with a UART and two GPIO pins.
- RS-485-to-UART TTL adapter. The component does not configure a direction-control GPIO, so use an adapter with automatic transmit/receive direction control.
- Battery RS-485 pin 1 is B-; pin 2 is A+.
- Serial format: 9600 baud, ASCII, 8 data bits, no parity, 1 stop bit (8N1).
- Configure each battery's DIP switches with a unique address. The first six switches encode the address in binary: address 1 is `ON-OFF-OFF-OFF-OFF-OFF`, address 2 is `OFF-ON-OFF-OFF-OFF-OFF`, and address 3 is `ON-ON-OFF-OFF-OFF-OFF`.

## Protocol notes (development)

The following captures and field map document the protocol investigation; they are not required to configure the ESPHome component.

The field offsets below were inferred from communication with `Tian_Power Energy Storage BMS V1.5.68-15.exe`.

## Command

The command must be sent in ASCII format.
- Address 1: "`~22014A42E00201FD28\r`"
- Address 2: "`~22024A42E00201FD27\r`"

### Captured response frame

The response is received in ASCII format.

```
~22014A0020C2002
01C138A0F0D070D0
70D070D070D080D0
70D070D070D070D0
70D070D070D070D0
70D0701360136013
6040140013601360
1360000000000640
1275C205B0026000
0000000000000102
3000000000000000
0000000000000000
000000000000000D
6B8.
```

### Data Interpretation

|       |     |                                                           |
| ----: | --: | --------------------------------------------------------- |
|   Len | Idx | Values                                                    |
|     1 |   0 | Start Frame: “\~” exclude from checksum                   |
|     2 |   1 | ? Always 22h                                              |
|     2 |   3 | Pack number: 01                                           |
|     2 |   5 | ? Always 74 => 4A                                         |
|     4 |   7 | ? Always 32 => 0020                                       |
|     2 |  11 | Frame Length before checksum: 194 => C2                   |
|     2 |  13 | ? Always 0 => 00                                          |
|     4 |  15 | SOC %: 82.20 => 201c multiply 0.01                        |
|     4 |  19 | Voltage: 50.02 => 138A multiply 0.01                      |
|     2 |  23 | Number of cells: 15 => 0F                                 |
| 4\*15 |  25 | Vol xx : 33.35 => 0D07 multiply 0.001                     |
|  4\*3 |  85 | xx\_Temp : 31.0 => 0136 multiply 0.01                     |
|     2 |  97 | Number of temp sensors: 04                                |
|  4\*4 |  99 | Temp xx: 31.0 => 0136 multiply 0.1                        |
|     4 | 115 | Current: 00.00 => 0000 multiply 0.01                      |
|     4 | 119 | ? Always => 0000                                          |
|     4 | 123 | SOH %: 100 => 0064h                                       |
|     2 | 127 | ? Always 1 => 01h                                         |
|     4 | 129 | NominalCap 100.76 => 275C multiply 0.01                   |
|     4 | 133 | RemainCap: 82.83 => 205B multiply 0.01                    |
|     4 | 137 | Cycles: 38 => 0026                                        |
|  4\*4 | 141 | ? 0000 0000 0000 0000                                     |
|     4 | 157 | ? Always 4143 => 1023h                                    |
| 4\*11 | 161 | ? 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000  |
|     2 | 205 | ? 00                                                      |
|     2 | 207 | Ending: D4 to D6 exclude from checksum                    |
|     2 | 209 | Checksum: B8                                              |
|     1 | 211 | End Frame “.” is 0x0D                                     |

Checksum: 8-bit two's complement (`0x100 - sum of ASCII bytes`), excluding the start marker `~` and the two bytes at offsets 207-208 (`D4` to `D6`).

### Fields not currently decoded

Alarm, protection, fault, and status flags (CHG, DSG, Limit, LED, Buzz, Heat) are not currently exposed as entities.

## Offline detection

If no valid frame (checksum OK) is received from a pack for `offline_timeout` (default `60s`, `0s` disables), the pack is marked offline:

- its sensors publish `NaN` (shown as `unknown` in Home Assistant) instead of keeping the last stale value
- its optional `binary_sensor` (`platform: sacredsun_rs485`, `pack: N`, device class connectivity) turns off
- the combined sensors (pack `-1`) are recalculated from the remaining online packs, or `NaN` if none are online

The pack comes back automatically with the next valid frame. Pick a timeout of at least 2 full polling rounds (`send_interval * pack_count * 2`), otherwise a warning is logged at boot.


## SOC correction (`soc_corrected`)

The BMS coulomb counter freezes at low current and never resyncs at the low end, so SOC can read 50%+ while the weakest cell is already near the knee. The component can publish a corrected SOC next to the raw one:

```yaml
sacredsun_rs485:
  soc_correction_max_current: 10      # A, trust cell voltage only below this current
  soc_correction_slew: 2              # max % the corrected SOC may drop per frame
  soc_correction_full_cell_mv: 3450   # full-charge anchor for the LOWEST cell, set below your absorption cell voltage

sensor:
  - platform: sacredsun_rs485
    pack: 0
    sensors:
      - {name: "Pack 1 SOC corrected", type: soc_corrected, unit_of_measurement: "%", filters: [{multiply: 0.01}]}
      - {name: "Pack 1 Cell Min", type: cell_min, unit_of_measurement: "mV"}
      - {name: "Pack 1 Cell Max", type: cell_max, unit_of_measurement: "mV"}
```

How it works: at low current the weakest cell voltage gives an upper bound for SOC (conservative LiFePO4 curve in `soc_ceiling_from_cell_mv`, tune it for your cells). Whenever the BMS SOC exceeds that bound, the difference is stored as an offset (it only grows) and subtracted from the BMS SOC. The offset is cleared when the BMS reports >= 99% AND the lowest cell is >= `soc_correction_full_cell_mv` (all cells at the top, so imbalance is taken into account; if balancing never lifts the weakest cell that far, the offset stays and the corrected SOC stays conservative). The offset is kept in RAM only (lost on reboot).
`remaining_kwh` raw unit is 0.1 Wh, use `multiply: 0.0001` for kWh.
