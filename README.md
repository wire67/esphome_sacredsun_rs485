# esphome_sacredsun_rs485
ESPHome device to fetch data from LiFePo4 SacredSun SCIFP48100 (Tian BMS) or equivalent via RS485 and push to Home Assistant Entities

## RS485

- Pin 1 is RS-485 B-
- Pin 2 is RS-485 A+
- Each DIP switch represents a bit in the slave ID value
  - address 1: ON-OFF-OFF-OFF-OFF-OFF
  - address 2: OFF-ON-OFF-OFF-OFF-OFF
  - address 3: ON-ON-OFF-OFF-OFF-OFF
- 9600 baud
- ASCII
- 8N1

## Frame decoding

Data mapping by looking at the comms with `Tian_Power Energy Storage BMS V1.5.68-15.exe`.

## Command

The command must be sent in ASCII format.
- Address 1: "`~22014A42E00201FD28\r`"
- Address 2: "`~22024A42E00201FD27\r`"

### Captured Sample Frame Response

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

<!--StartFragment-->

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

<!--EndFragment-->

Checksum : CheckSum8 2s Complement, 0x100 - Sum Of ASCII Bytes, exclude start “~” and ending “D4 to D6”.

### Todo

Still remaining is the decoding of the following

1. Alarm
2. Protection
3. Fault
4. Statuses: CHG, DSG, Limit, LED, Buzz, Heat