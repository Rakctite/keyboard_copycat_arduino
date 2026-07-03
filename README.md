# Keyboard Copycat Arduino Firmware

Arduino Nano ESP32 firmware that receives 8-byte keyboard reports from a Windows sender program over BLE GATT and forwards them as USB HID keyboard output.

## Architecture

```text
Windows sender program
  -> BLE GATT write, 8-byte keyboard report
  -> Arduino Nano ESP32
  -> USB HID keyboard output
  -> USB-connected target device
```

The Arduino is not paired with Windows as a normal Bluetooth keyboard. The Windows sender program connects to the custom BLE service and writes report bytes.

## BLE Protocol

Device name:

```text
KeyboardBridge
```

Service UUID:

```text
7f2b4c00-7b64-4c0d-9b7a-1e0f3c9a0001
```

Report characteristic UUID:

```text
7f2b4c01-7b64-4c0d-9b7a-1e0f3c9a0001
```

Characteristic properties:

```text
write
write without response
```

Payload format:

```text
byte 0: modifier bitmask
byte 1: reserved
byte 2-7: up to six USB HID keyboard usage IDs
```

Example `a` press:

```text
00 00 04 00 00 00 00 00
```

Release all:

```text
00 00 00 00 00 00 00 00
```

## Safety

The firmware sends release-all when BLE disconnects or when no report is received for `KEY_RELEASE_TIMEOUT_MS`.

## Build

```bash
pio run
```

## Upload

```bash
pio run --target upload
pio device monitor
```

## Manual Test

1. Flash the firmware.
2. Connect Arduino USB to the target device.
3. Connect to BLE device `KeyboardBridge` from a BLE client.
4. Write `00 00 04 00 00 00 00 00` to the report characteristic.
5. Confirm the USB target receives `a`.
6. Write `00 00 00 00 00 00 00 00`.
7. Confirm all keys release.
