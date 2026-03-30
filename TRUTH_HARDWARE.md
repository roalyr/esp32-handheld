0. Main: ESP32-S2-mini (2MB PSRAM, 4MB FLASH).

0.1. SPI bus.
| Function    | GPIO |
| ----------- | ---- |
| MOSI        | 35   |
| MISO        | 37   |
| SCK         | 36   |

1. Screen: LCD 12864-20M 128X64 ST7920.
| **ESP32-S2 Mini Pin** | **LCD Module Pin** | **Function**     | **Connection Note**               |
| --------------------- | ------------------ | ---------------- | --------------------------------- |
| **GND**               | 1 (GND)            | Ground           | Common Ground                     |
| **VBUS (5V)**         | 2 (VCC)            | Logic Supply     | Use 5V (ST7920 standard)          |
| —                     | 3 (V0)             | Contrast         | Connect to 10k Pot wiper (or GND) |
| **GPIO 38**           | 4 (RS)             | **CS**           | Chip Select                       |
| **GPIO 35**           | 5 (R/W)            | **SID**          | MOSI (Serial Data)                |
| **GPIO 36**           | 6 (E)              | **SCLK**         | SCK (Serial Clock)                |
| **GND**               | 15 (PSB)           | Interface Select | **Tie to GND** for SPI mode       |
| **3V3** or GPIO       | 17 (RST)           | Reset            | Pull High to 3.3V to run          |
| **GPIO 40**           | 19 (BLA)           | Backlight +      | PWM Anode                         |
| **GND**               | 20 (BLK)           | Backlight -      | Cathode                           |

2. Keyboard: 4 rows by 5 columns matrix.
| Row              | GPIO |
| ---------------- | ---- |
| 4th row (bottom) | 9    |
| 3rd row          | 7    |
| 2nd row          | 5    |
| 1st row (top)    | 3    |

| Columns     | GPIO |
| ----------- | ---- |
| 1st (right) | 8    |
| 2nd         | 6    |
| 3rd         | 4    |
| 4th         | 2    |
| 5th (left)  | 1    |

3. SDcard module (SPI bus)
| Columns     | GPIO |
| ----------- | ---- |
| CS          | 39   |

4. Buzzers (Not yet wired).

5. Free pins (so far): 11, 12; 10, 13, 14; 15; 16, 17, 18, 21, 33, 34
