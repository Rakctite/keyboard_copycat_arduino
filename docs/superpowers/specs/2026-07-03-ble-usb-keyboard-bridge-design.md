# BLE to USB Keyboard Bridge Firmware Design

## Goal

Build Arduino Nano ESP32 firmware that receives keyboard reports from a Windows sender program over Bluetooth Low Energy and emits the same key state as a USB HID keyboard to the device connected by USB.

The firmware is not a standard Bluetooth keyboard for Windows pairing. Windows will use a separate sender program that connects to a custom BLE GATT service and writes keyboard reports.

## Hardware and Runtime Roles

- Board: Arduino Nano ESP32.
- USB role: USB HID keyboard device connected to the target machine or device.
- Bluetooth role: BLE peripheral / GATT server connected by the Windows sender program.
- Windows role: BLE central that captures local keyboard events and writes report data to the Arduino.

Data flow:

```text
Windows sender app
  -> BLE GATT keyboard report write
  -> Arduino Nano ESP32 firmware
  -> USB HID keyboard report
  -> USB-connected target device
```

## BLE Protocol

The firmware exposes one custom BLE service with one writable characteristic.

- Service UUID: fixed project UUID, defined in firmware constants.
- Characteristic UUID: fixed project UUID, defined in firmware constants.
- Characteristic properties: write and write without response.
- Payload length: exactly 8 bytes.

The 8-byte payload uses the USB HID boot keyboard report layout:

```text
byte 0: modifier bitmask
byte 1: reserved, must be 0
byte 2: keycode slot 1
byte 3: keycode slot 2
byte 4: keycode slot 3
byte 5: keycode slot 4
byte 6: keycode slot 5
byte 7: keycode slot 6
```

Modifier bits:

```text
bit 0: left ctrl
bit 1: left shift
bit 2: left alt
bit 3: left gui / windows
bit 4: right ctrl
bit 5: right shift
bit 6: right alt
bit 7: right gui / windows
```

The Windows sender is responsible for converting Windows virtual keys into USB HID usage IDs and maintaining the current pressed-key state. The firmware forwards reports and does not infer press/release from text characters.

## USB HID Output

The firmware initializes native USB and the USB HID keyboard interface during setup. Each valid BLE report is sent to the USB host as the current keyboard state.

Required behavior:

- A non-zero report presses the represented key state.
- An all-zero report releases every key.
- Repeated identical reports are allowed.
- Reports with invalid length are ignored and do not change USB state.

## Safety Behavior

Stuck keys are the main failure mode. The firmware sends an all-zero release report when:

- The BLE client disconnects.
- No BLE report is received for a configurable timeout.
- The firmware starts USB HID output.

Default timeout: 3000 ms.

The timeout should only release keys. It should not disconnect BLE or restart the device.

## Firmware Components

### BleReportReceiver

Responsibilities:

- Start BLE device advertising.
- Expose the custom service and writable report characteristic.
- Track connection state.
- Validate incoming writes are exactly 8 bytes.
- Store the latest valid report and mark it ready for USB output.

Dependencies:

- Arduino BLE stack available for Arduino Nano ESP32.
- Firmware constants for BLE name, service UUID, and characteristic UUID.

### UsbKeyboardOutput

Responsibilities:

- Start USB and keyboard HID.
- Send raw 8-byte keyboard reports.
- Send all-zero release report.

Dependencies:

- Arduino Nano ESP32 USB HID support.

### Main Loop

Responsibilities:

- Initialize Serial, USB HID, and BLE.
- Forward valid reports from BLE to USB.
- Enforce the release timeout.
- Log concise connection and report validation events over Serial.

## Configuration

Configuration should be kept in `include/config.h` or a dedicated firmware header:

- BLE device name: `KeyboardBridge`.
- BLE service UUID.
- BLE report characteristic UUID.
- Key release timeout in milliseconds.
- Optional debug logging flag.

The previous MQTT/Modbus configuration is out of scope for this firmware target and should not be kept active in the new bridge firmware path.

## Testing

Firmware-level verification:

- Build with PlatformIO for `arduino_nano_esp32`.
- Unit or contract tests should verify the configured UUIDs, 8-byte report requirement, timeout constant, and absence of MQTT/Modbus dependencies from the active firmware.

Manual hardware verification:

1. Flash the firmware to Arduino Nano ESP32.
2. Connect Arduino USB to a target machine that can receive keyboard input.
3. Use a BLE client to connect to `KeyboardBridge`.
4. Write `00 00 04 00 00 00 00 00` to the report characteristic.
5. Confirm the USB target receives `a`.
6. Write `00 00 00 00 00 00 00 00`.
7. Confirm the key releases.
8. Disconnect BLE while a modifier is pressed and confirm the firmware sends release.

## Non-Goals

- No Windows sender program in this firmware step.
- No standard Bluetooth HID pairing with Windows.
- No text protocol such as `DOWN:A` or `UP:A`.
- No MQTT, WiFi, or Modbus behavior in the bridge firmware.
- No support for more than six simultaneous non-modifier keys in the first version.
