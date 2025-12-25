0. GPIO38 is broken (lefted pad) on the current board.
System Reserved (Unused): 19, 20 (USB), 48 (RGB LED).

1. Screen (Left Side - SPI)
| Function      |        GPIO Pin |  Wired?   |
|---------------|-----------------|-----------|
| SCL (Clock)   | 12 (Shared SPI) |  yes      |
| SI (MOSI)     | 11 (Shared SPI) |  yes      |
| CS            | 10              |  yes      |
| DC (RS)       | 13              |  yes      |
| RST (RSE)     | 14              |  yes      |
| Backlight (A) | 9               |  yes      |

2. Keyboard (Right Side - 4x5 Matrix)
| Function    |        GPIO Pins                                          |  Wired?   |
|-------------|-----------------------------------------------------------|-----------|
| Rows        | 42(1st), 41(2nd), 40(3rd), 39(4th)                        |  yes      |
| Columns     | 1(1st), 2(2nd), 43(TX)(5th), 44(RX)(4th), 45(near 0)(3rd) |  yes      |

3. Buzzers (Left Side - PWM)
| Function      | GPIO Pins |  Wired?   |
|---------------|-----------|-----------|
| Synth 1, 2, 3 | 4, 5, 6   |  not yet  |

4. SD Card (Shared SPI Cluster)
| Function | GPIO Pin                |  Wired?   |
|----------|-------------------------|-----------|
| SCK      | 12 (Shared with Screen) |  not yet  |
| MOSI     | 11 (Shared with Screen) |  not yet  |
| MISO     | 8                       |  not yet  |
| CS       | 7                       |  not yet  |
