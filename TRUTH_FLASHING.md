# ESP32-S2-Mini Flashing Guide (LOLIN S2 Mini)

## Quick Reference

```
1. Build:           pio run
2. Enter DFU mode:  hold BOOT → plug in USB (or hold BOOT → tap RST)
3. Flash:           bash scripts/flash.sh
4. Boot:            unplug and re-plug the board (no buttons held)
```

Or combined: `pio run && bash scripts/flash.sh` (board must already be in DFU mode).

`pio run -t upload` also works — it calls `scripts/flash.sh` via the custom upload command in `platformio.ini`.

---

## Why DFU (Not Serial)

The LOLIN S2 Mini uses **native USB** (USB-OTG built into the ESP32-S2 chip). There is
**no USB-to-UART bridge** (no CP2102, no CH340). This has major implications:

| Method | Status | Why |
|---|---|---|
| `esptool --before default-reset` | **FAILS** | No DTR/RTS hardware lines to toggle — native USB CDC has no physical reset circuit |
| `esptool --before usb-reset` | **FAILS** | USB reset disconnects the device; bus-powered board loses power and never re-enumerates in bootloader |
| `esptool --before no-reset` on PID 0002 | **FAILS** | PID 0002 factory/ROM firmware exposes CDC but does not respond to esptool serial protocol |
| **DFU via `dfu-util`** | **WORKS** | PID 0002 exposes a DFU interface (Application Specific Interface, subclass 1). `dfu-util` speaks this protocol natively |

**Bottom line:** On bus-powered ESP32-S2 boards, DFU is the only reliable flash method.

---

## Prerequisites

### Tools

| Tool | Version | Install | Purpose |
|---|---|---|---|
| PlatformIO | any | VS Code extension or `pip install platformio` | Build system |
| esptool | ≥ 5.x | `~/.platformio/penv/bin/pip install esptool` | Binary merging (`merge-bin`) |
| dfu-util | ≥ 0.11 | `sudo apt-get install dfu-util` | DFU flashing |

### udev Rules (Linux)

Needed so that `dfu-util` can access the device without root. Add to
`/etc/udev/rules.d/99-platformio-udev.rules`:

```
# Espressif ESP32-S2 — blanket rule for all PIDs
ATTRS{idVendor}=="303a", MODE="0666", ENV{ID_MM_DEVICE_IGNORE}="1", ENV{ID_MM_PORT_IGNORE}="1"
```

Then reload:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

The `ID_MM_DEVICE_IGNORE` / `ID_MM_PORT_IGNORE` flags prevent ModemManager from
probing the device (important on laptops with built-in mobile broadband modems).

---

## USB Device Identity

The ESP32-S2 presents different USB identities depending on its state:

| State | USB VID:PID | `lsusb` name | Serial port | Notes |
|---|---|---|---|---|
| **ROM bootloader / factory firmware** | `303a:0002` | `Espressif ESP32-S2` | `/dev/ttyACM*` (but serial protocol non-functional) | DFU interface available |
| **Our Arduino firmware running** | `303a:80c2` | `WEMOS.CC LOLIN-S2-MINI` | `/dev/ttyACM*` (CDC working) | Serial monitor works here |

### Port Numbering (Host-Specific)

On the development laptop (Lenovo X230), `/dev/ttyACM0`–`ACM2` are consumed by the
built-in Ericsson H5321 mobile broadband modem. The ESP32 typically appears on
`/dev/ttyACM3`. This may differ on other machines.

---

## Flashing Procedure (Step by Step)

### 1. Build

```bash
pio run -e lolin_s2_mini
```

Build artifacts land in `.pio/build/lolin_s2_mini/`:
- `bootloader.bin` — second-stage bootloader (at flash offset `0x1000`)
- `partitions.bin` — partition table (at `0x8000`)
- `firmware.bin` — application (at `0x10000`)

### 2. Enter DFU Mode

**Method A — Cold boot into DFU (preferred):**
1. Unplug the board
2. Hold the **BOOT** button
3. Plug in USB while holding BOOT
4. Release BOOT after ~1 second
5. Verify: `lsusb | grep 303a` should show `303a:0002`

**Method B — Warm reset into DFU:**
1. Board is plugged in and running (PID `80c2`)
2. Hold the **BOOT** button
3. Tap **RST** (release RST, keep holding BOOT)
4. Release BOOT after ~1 second
5. Verify: `lsusb | grep 303a` should show `303a:0002`

### 3. Flash

```bash
bash scripts/flash.sh
```

What the script does internally:
1. **DFU detach** — `dfu-util -d 303a:0002 -e` switches the device from DFU "Runtime"
   mode to DFU "Download" mode (device re-enumerates, device number changes)
2. **Merge binaries** — `esptool merge-bin` combines bootloader + partitions + firmware
   into a single `merged.bin` at correct offsets
3. **DFU download** — `dfu-util -D merged.bin -a 0` writes the merged binary to flash

### 4. Boot the New Firmware

**The board does not auto-reboot after DFU flash.** You must:
1. **Unplug** the USB cable
2. **Re-plug** normally (no buttons held)

The board boots, our firmware runs, and it re-enumerates as PID `80c2`
(`WEMOS.CC LOLIN-S2-MINI`).

---

## Serial Monitor

Once booted (PID `80c2`), the serial monitor works normally:

```bash
pio device monitor
```

Or: `monitor_port = /dev/ttyACM3` in `platformio.ini` (adjust port for your machine).

---

## Troubleshooting

### `dfu-util` says "Found Runtime" but flash fails
The device is in DFU Runtime mode, not Download mode. The `scripts/flash.sh` script
handles the detach automatically. If running manually, issue `dfu-util -d 303a:0002 -e`
first, wait 2 seconds, then flash.

### Device disappears from USB entirely after reset
This is expected on bus-powered boards. The USB reset causes the host to briefly drop
VBUS, and the ESP32-S2 loses power. The chip cannot re-enumerate in bootloader mode
without power. Solution: always use the **cold boot** method (unplug → hold BOOT →
plug in).

### `lsusb` shows `303a:0002` but `esptool` can't connect
PID `0002` with factory/ROM firmware exposes a CDC interface that does **not** speak the
esptool serial protocol. Use DFU (`dfu-util`), not `esptool`, to flash this device state.

### Wrong serial port
The port number depends on what other ACM devices exist on the host. Use
`ls /dev/ttyACM*` and `lsusb` to identify the correct one.

### `dfu-util: error detaching`
This warning during `dfu-util -e` is normal — the device detaches and re-enumerates,
which interrupts the USB transaction. The script handles this with `|| true`.

---

## Architecture Notes

### Why Not `upload_protocol = esptool` (PlatformIO Default)

PlatformIO's built-in esptool integration (v4.x) does not support the `--before usb-reset`
flag needed for ESP32-S2 native USB. Even esptool v5.x with `usb-reset` fails on
bus-powered boards because the chip loses power during the USB reset cycle and never
re-enumerates. DFU is the only method that works reliably because:
- The chip is already powered and in bootloader mode before flashing starts
- The DFU protocol doesn't require a mid-flash USB disconnect/reconnect
- `dfu-util` has native support for the ESP32-S2 ROM bootloader's DFU interface

### Flash Memory Layout (ESP32-S2, 4MB)

| Offset | Content |
|---|---|
| `0x1000` | Second-stage bootloader |
| `0x8000` | Partition table |
| `0x10000` | Application firmware |

The `esptool merge-bin` command packs all three into a single binary starting at offset 0,
with zero-padding between sections. This is what `dfu-util` writes to flash.
