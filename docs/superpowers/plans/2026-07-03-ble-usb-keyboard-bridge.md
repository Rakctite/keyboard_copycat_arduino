# BLE USB Keyboard Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Arduino Nano ESP32 firmware that receives 8-byte keyboard reports over BLE GATT and forwards them as USB HID keyboard output.

**Architecture:** The firmware exposes a custom BLE GATT service with one writable report characteristic. BLE write callbacks validate exactly 8 bytes and hand the report to the main loop, which sends it through a small USB keyboard output wrapper and releases all keys on disconnect or timeout.

**Tech Stack:** PlatformIO, Arduino framework for `arduino_nano_esp32`, Arduino-ESP32 BLE API (`BLEDevice.h`), Arduino Nano ESP32 USB HID (`USB.h`, `USBHIDKeyboard.h`), pytest contract tests.

---

## File Structure

- Modify `platformio.ini`: remove MQTT/Modbus libraries and keep only firmware dependencies needed by the BLE keyboard bridge.
- Replace `include/config.h`: bridge constants for BLE name, UUIDs, report size, timeout, and debug logging.
- Replace `include/config.example.h`: public example matching the new bridge firmware.
- Create `src/keyboard_report.h`: shared report constants and helper functions for validation/release reports.
- Create `src/ble_report_receiver.h`: BLE GATT server setup and callback declarations.
- Create `src/ble_report_receiver.cpp`: BLE implementation, connection tracking, report buffering.
- Create `src/usb_keyboard_output.h`: USB HID output wrapper declaration.
- Create `src/usb_keyboard_output.cpp`: USB initialization, raw report sending, release-all behavior.
- Replace `src/main.cpp`: firmware startup, BLE-to-USB forwarding loop, timeout safety.
- Replace `README.md`: setup, protocol, build, upload, and manual BLE test instructions.
- Replace `test/test_contract.py`: contract tests for active bridge firmware.

## Task 1: Convert Configuration and Contracts

**Files:**
- Modify: `include/config.h`
- Modify: `include/config.example.h`
- Modify: `test/test_contract.py`

- [ ] **Step 1: Replace the contract tests first**

Write `test/test_contract.py`:

```python
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_bridge_config_documents_ble_and_report_contract():
    config = read("include/config.example.h")

    assert 'BLE_DEVICE_NAME "KeyboardBridge"' in config
    assert "BLE_KEYBOARD_SERVICE_UUID" in config
    assert "BLE_KEYBOARD_REPORT_CHAR_UUID" in config
    assert "KEYBOARD_REPORT_SIZE 8" in config
    assert "KEY_RELEASE_TIMEOUT_MS 3000UL" in config
    assert "BRIDGE_DEBUG_LOG 1" in config


def test_active_config_is_bridge_firmware_not_mqtt_modbus():
    config = read("include/config.h")

    assert 'BLE_DEVICE_NAME "KeyboardBridge"' in config
    assert "KEYBOARD_REPORT_SIZE 8" in config
    assert "WIFI_SSID" not in config
    assert "MQTT_HOST" not in config
    assert "MODBUS_TAGS" not in config


def test_firmware_uses_ble_receiver_and_usb_output_layers():
    main = read("src/main.cpp")

    assert '#include "ble_report_receiver.h"' in main
    assert '#include "usb_keyboard_output.h"' in main
    assert "BleReportReceiver bleReceiver;" in main
    assert "UsbKeyboardOutput usbOutput;" in main
    assert "KEY_RELEASE_TIMEOUT_MS" in main
    assert "releaseAll" in main


def test_no_legacy_network_or_modbus_dependencies_in_active_firmware():
    platformio = read("platformio.ini")
    main = read("src/main.cpp")

    assert "PubSubClient" not in platformio
    assert "modbus-esp8266" not in platformio
    assert "#include <WiFi" not in main
    assert "Serial1.begin" not in main
    assert "MQTT" not in main
```

- [ ] **Step 2: Run tests and verify they fail**

Run:

```bash
pytest test/test_contract.py -q
```

Expected: FAIL because the current config and firmware still contain legacy MQTT/Modbus config and no BLE bridge layers.

- [ ] **Step 3: Replace `include/config.example.h`**

```cpp
#pragma once

#define BLE_DEVICE_NAME "KeyboardBridge"

#define BLE_KEYBOARD_SERVICE_UUID "7f2b4c00-7b64-4c0d-9b7a-1e0f3c9a0001"
#define BLE_KEYBOARD_REPORT_CHAR_UUID "7f2b4c01-7b64-4c0d-9b7a-1e0f3c9a0001"

#define KEYBOARD_REPORT_SIZE 8
#define KEY_RELEASE_TIMEOUT_MS 3000UL

#define BRIDGE_DEBUG_LOG 1
```

- [ ] **Step 4: Replace `include/config.h` with the same bridge defaults**

Use the same contents as `include/config.example.h` unless the local device needs a different BLE name.

- [ ] **Step 5: Run contract tests for this task**

Run:

```bash
pytest test/test_contract.py -q
```

Expected: still FAIL because source files have not been created yet, but config-related assertions pass.

## Task 2: Add Keyboard Report Helpers

**Files:**
- Create: `src/keyboard_report.h`
- Modify: `test/test_contract.py`

- [ ] **Step 1: Add report helper contract tests**

Append to `test/test_contract.py`:

```python
def test_keyboard_report_helper_defines_size_and_release_report():
    helper = read("src/keyboard_report.h")

    assert "isValidKeyboardReportLength" in helper
    assert "makeReleaseReport" in helper
    assert "KEYBOARD_REPORT_SIZE" in helper
    assert "memset(report, 0, KEYBOARD_REPORT_SIZE)" in helper
```

- [ ] **Step 2: Run tests and verify the new test fails**

Run:

```bash
pytest test/test_contract.py::test_keyboard_report_helper_defines_size_and_release_report -q
```

Expected: FAIL because `src/keyboard_report.h` does not exist.

- [ ] **Step 3: Create `src/keyboard_report.h`**

```cpp
#pragma once

#include <Arduino.h>
#include <string.h>
#include "config.h"

inline bool isValidKeyboardReportLength(size_t length) {
  return length == KEYBOARD_REPORT_SIZE;
}

inline void makeReleaseReport(uint8_t report[KEYBOARD_REPORT_SIZE]) {
  memset(report, 0, KEYBOARD_REPORT_SIZE);
}

inline bool isReleaseReport(const uint8_t report[KEYBOARD_REPORT_SIZE]) {
  for (size_t i = 0; i < KEYBOARD_REPORT_SIZE; ++i) {
    if (report[i] != 0) {
      return false;
    }
  }
  return true;
}
```

- [ ] **Step 4: Run the helper test**

Run:

```bash
pytest test/test_contract.py::test_keyboard_report_helper_defines_size_and_release_report -q
```

Expected: PASS.

## Task 3: Add USB Keyboard Output Wrapper

**Files:**
- Create: `src/usb_keyboard_output.h`
- Create: `src/usb_keyboard_output.cpp`
- Modify: `test/test_contract.py`

- [ ] **Step 1: Add USB wrapper contract test**

Append to `test/test_contract.py`:

```python
def test_usb_keyboard_output_wraps_usb_hid_keyboard():
    header = read("src/usb_keyboard_output.h")
    source = read("src/usb_keyboard_output.cpp")

    assert "class UsbKeyboardOutput" in header
    assert "void begin()" in header
    assert "void sendReport" in header
    assert "void releaseAll()" in header
    assert "#include <USB.h>" in source
    assert "#include <USBHIDKeyboard.h>" in source
    assert "USB.begin()" in source
    assert "Keyboard.begin()" in source
```

- [ ] **Step 2: Run tests and verify the new test fails**

Run:

```bash
pytest test/test_contract.py::test_usb_keyboard_output_wraps_usb_hid_keyboard -q
```

Expected: FAIL because the USB wrapper files do not exist.

- [ ] **Step 3: Create `src/usb_keyboard_output.h`**

```cpp
#pragma once

#include <Arduino.h>
#include "keyboard_report.h"

class UsbKeyboardOutput {
 public:
  void begin();
  void sendReport(const uint8_t report[KEYBOARD_REPORT_SIZE]);
  void releaseAll();
};
```

- [ ] **Step 4: Create `src/usb_keyboard_output.cpp`**

```cpp
#include "usb_keyboard_output.h"

#include <USB.h>
#include <USBHIDKeyboard.h>
#include "config.h"

static USBHIDKeyboard Keyboard;

void UsbKeyboardOutput::begin() {
  USB.begin();
  Keyboard.begin();
  releaseAll();
}

void UsbKeyboardOutput::sendReport(const uint8_t report[KEYBOARD_REPORT_SIZE]) {
  Keyboard.sendReport(report, KEYBOARD_REPORT_SIZE);
#if BRIDGE_DEBUG_LOG
  Serial.print("[usb] report sent release=");
  Serial.println(isReleaseReport(report) ? "yes" : "no");
#endif
}

void UsbKeyboardOutput::releaseAll() {
  uint8_t report[KEYBOARD_REPORT_SIZE];
  makeReleaseReport(report);
  sendReport(report);
}
```

- [ ] **Step 5: Build to verify the raw USB HID API**

Run:

```bash
pio run
```

Expected: PASS if `USBHIDKeyboard::sendReport(const uint8_t*, size_t)` is available. If this fails because the exact raw report method differs, inspect the installed `USBHIDKeyboard.h` and replace only `UsbKeyboardOutput::sendReport` with the equivalent raw HID report call while preserving the class interface and tests.

## Task 4: Add BLE Report Receiver

**Files:**
- Create: `src/ble_report_receiver.h`
- Create: `src/ble_report_receiver.cpp`
- Modify: `test/test_contract.py`

- [ ] **Step 1: Add BLE receiver contract test**

Append to `test/test_contract.py`:

```python
def test_ble_report_receiver_exposes_custom_gatt_write_endpoint():
    header = read("src/ble_report_receiver.h")
    source = read("src/ble_report_receiver.cpp")

    assert "class BleReportReceiver" in header
    assert "bool takeReport" in header
    assert "bool isConnected" in header
    assert "#include <BLEDevice.h>" in source
    assert "BLE_KEYBOARD_SERVICE_UUID" in source
    assert "BLE_KEYBOARD_REPORT_CHAR_UUID" in source
    assert "PROPERTY_WRITE" in source
    assert "PROPERTY_WRITE_NR" in source
    assert "isValidKeyboardReportLength" in source
```

- [ ] **Step 2: Run tests and verify the new test fails**

Run:

```bash
pytest test/test_contract.py::test_ble_report_receiver_exposes_custom_gatt_write_endpoint -q
```

Expected: FAIL because BLE receiver files do not exist.

- [ ] **Step 3: Create `src/ble_report_receiver.h`**

```cpp
#pragma once

#include <Arduino.h>
#include "keyboard_report.h"

class BleReportReceiver {
 public:
  void begin();
  bool takeReport(uint8_t report[KEYBOARD_REPORT_SIZE]);
  bool isConnected() const;
  uint32_t lastReportMillis() const;
  void markDisconnectedReleaseHandled();
  bool needsDisconnectRelease() const;

 private:
  friend class BridgeServerCallbacks;
  friend class ReportCharacteristicCallbacks;

  void setConnected(bool connected);
  void receiveReport(const uint8_t* data, size_t length);

  volatile bool connected_ = false;
  volatile bool reportReady_ = false;
  volatile bool disconnectReleaseNeeded_ = false;
  uint8_t latestReport_[KEYBOARD_REPORT_SIZE] = {0};
  uint32_t lastReportMillis_ = 0;
};
```

- [ ] **Step 4: Create `src/ble_report_receiver.cpp`**

```cpp
#include "ble_report_receiver.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "config.h"

static BleReportReceiver* activeReceiver = nullptr;

class BridgeServerCallbacks : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer* server) override {
    (void)server;
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(true);
    }
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] connected");
#endif
  }

  void onDisconnect(BLEServer* server) override {
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(false);
    }
    server->getAdvertising()->start();
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] disconnected; advertising restarted");
#endif
  }
};

class ReportCharacteristicCallbacks : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic* characteristic) override {
    if (activeReceiver == nullptr) {
      return;
    }
    std::string value = characteristic->getValue();
    activeReceiver->receiveReport(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size());
  }
};

void BleReportReceiver::begin() {
  activeReceiver = this;
  BLEDevice::init(BLE_DEVICE_NAME);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new BridgeServerCallbacks());

  BLEService* service = server->createService(BLE_KEYBOARD_SERVICE_UUID);
  BLECharacteristic* reportCharacteristic = service->createCharacteristic(
      BLE_KEYBOARD_REPORT_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  reportCharacteristic->setCallbacks(new ReportCharacteristicCallbacks());
  reportCharacteristic->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_KEYBOARD_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

#if BRIDGE_DEBUG_LOG
  Serial.print("[ble] advertising as ");
  Serial.println(BLE_DEVICE_NAME);
#endif
}

bool BleReportReceiver::takeReport(uint8_t report[KEYBOARD_REPORT_SIZE]) {
  if (!reportReady_) {
    return false;
  }

  noInterrupts();
  memcpy(report, latestReport_, KEYBOARD_REPORT_SIZE);
  reportReady_ = false;
  interrupts();
  return true;
}

bool BleReportReceiver::isConnected() const {
  return connected_;
}

uint32_t BleReportReceiver::lastReportMillis() const {
  return lastReportMillis_;
}

void BleReportReceiver::markDisconnectedReleaseHandled() {
  disconnectReleaseNeeded_ = false;
}

bool BleReportReceiver::needsDisconnectRelease() const {
  return disconnectReleaseNeeded_;
}

void BleReportReceiver::setConnected(bool connected) {
  connected_ = connected;
  if (!connected) {
    disconnectReleaseNeeded_ = true;
  }
}

void BleReportReceiver::receiveReport(const uint8_t* data, size_t length) {
  if (!isValidKeyboardReportLength(length)) {
#if BRIDGE_DEBUG_LOG
    Serial.print("[ble] ignored invalid report length=");
    Serial.println(length);
#endif
    return;
  }

  noInterrupts();
  memcpy(latestReport_, data, KEYBOARD_REPORT_SIZE);
  lastReportMillis_ = millis();
  reportReady_ = true;
  interrupts();

#if BRIDGE_DEBUG_LOG
  Serial.println("[ble] report received");
#endif
}
```

- [ ] **Step 5: Run BLE receiver contract test**

Run:

```bash
pytest test/test_contract.py::test_ble_report_receiver_exposes_custom_gatt_write_endpoint -q
```

Expected: PASS.

- [ ] **Step 6: Build and fix only API mismatches**

Run:

```bash
pio run
```

Expected: PASS. If the Arduino-ESP32 BLE API returns `String` instead of `std::string` for `getValue()`, update `onWrite` to use the installed API's byte pointer and length accessors while preserving the exact 8-byte validation behavior.

## Task 5: Replace Main Firmware Loop

**Files:**
- Modify: `src/main.cpp`
- Modify: `platformio.ini`

- [ ] **Step 1: Replace `platformio.ini` dependencies**

Use:

```ini
; PlatformIO Project Configuration File

[env:arduino_nano_esp32]
platform = espressif32@6.5.0
board = arduino_nano_esp32
framework = arduino
monitor_speed = 115200
```

- [ ] **Step 2: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include "ble_report_receiver.h"
#include "config.h"
#include "usb_keyboard_output.h"

BleReportReceiver bleReceiver;
UsbKeyboardOutput usbOutput;

static bool timeoutReleaseSent = true;

static void sendReleaseForSafety(const char* reason) {
#if BRIDGE_DEBUG_LOG
  Serial.print("[safety] releaseAll reason=");
  Serial.println(reason);
#endif
  usbOutput.releaseAll();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  usbOutput.begin();
  bleReceiver.begin();

  timeoutReleaseSent = true;
#if BRIDGE_DEBUG_LOG
  Serial.println("[bridge] ready");
#endif
}

void loop() {
  uint8_t report[KEYBOARD_REPORT_SIZE];

  if (bleReceiver.takeReport(report)) {
    usbOutput.sendReport(report);
    timeoutReleaseSent = isReleaseReport(report);
  }

  if (bleReceiver.needsDisconnectRelease()) {
    sendReleaseForSafety("ble_disconnect");
    timeoutReleaseSent = true;
    bleReceiver.markDisconnectedReleaseHandled();
  }

  const uint32_t lastReport = bleReceiver.lastReportMillis();
  if (bleReceiver.isConnected() && lastReport > 0 && !timeoutReleaseSent) {
    const uint32_t elapsed = millis() - lastReport;
    if (elapsed >= KEY_RELEASE_TIMEOUT_MS) {
      sendReleaseForSafety("timeout");
      timeoutReleaseSent = true;
    }
  }

  delay(1);
}
```

- [ ] **Step 3: Run full contract tests**

Run:

```bash
pytest test/test_contract.py -q
```

Expected: PASS.

- [ ] **Step 4: Build firmware**

Run:

```bash
pio run
```

Expected: PASS.

## Task 6: Update README for the New Firmware

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Replace README**

```markdown
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
```

- [ ] **Step 2: Run tests and build**

Run:

```bash
pytest test/test_contract.py -q
pio run
```

Expected: both PASS.

## Task 7: Prepare Repository for GitHub Remote

**Files:**
- Modify: local git metadata only

- [ ] **Step 1: Initialize git if needed**

Run:

```bash
git rev-parse --is-inside-work-tree
```

Expected now: FAIL in the current local folder. Then run:

```bash
git init
git branch -M main
git remote add origin https://github.com/Rakctite/keyboard_copycat_arduino.git
```

- [ ] **Step 2: Commit firmware work**

Run:

```bash
git add README.md platformio.ini include src test docs
git commit -m "feat: add BLE to USB keyboard bridge firmware"
```

Expected: commit succeeds.

- [ ] **Step 3: Push when credentials are available**

Run:

```bash
git push -u origin main
```

Expected: push succeeds if GitHub credentials for `Rakctite/keyboard_copycat_arduino` are configured on this machine.

## Self-Review

- Spec coverage: BLE GATT service, 8-byte HID report, USB HID output, disconnect release, timeout release, config constants, tests, and docs are covered.
- Scope: This plan only implements the Arduino firmware. The Windows sender repo remains a separate follow-up project.
- Known risk: `USBHIDKeyboard::sendReport(report, size)` may need adjustment to the exact Arduino Nano ESP32 USB HID API installed by PlatformIO. Task 3 isolates that risk behind `UsbKeyboardOutput`.
