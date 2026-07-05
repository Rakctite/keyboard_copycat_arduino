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
    assert "USB_KEYBOARD_OUTPUT_ENABLED" in config
    assert "BRIDGE_DEBUG_LOG 1" in config


def test_active_config_is_bridge_firmware_not_mqtt_modbus():
    config = read("include/config.h")

    assert 'BLE_DEVICE_NAME "KeyboardBridge"' in config
    assert "KEYBOARD_REPORT_SIZE 8" in config
    assert "WIFI_SSID" not in config
    assert "MQTT_HOST" not in config
    assert "MODBUS_TAGS" not in config


def test_keyboard_report_helper_defines_size_and_release_report():
    helper = read("src/keyboard_report.h")

    assert "isValidKeyboardReportLength" in helper
    assert "makeReleaseReport" in helper
    assert "KEYBOARD_REPORT_SIZE" in helper
    assert "memset(report, 0, KEYBOARD_REPORT_SIZE)" in helper


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
    assert "PROPERTY_WRITE_NR" not in source
    assert "isValidKeyboardReportLength" in source


def test_ble_report_receiver_logs_received_report_bytes_for_debugging():
    source = read("src/ble_report_receiver.cpp")

    assert "printReportHex" in source
    assert 'Serial.print("[ble] report bytes=")' in source
    assert "if (data[i] < 0x10)" in source


def test_firmware_uses_ble_receiver_and_usb_output_layers():
    main = read("src/main.cpp")

    assert '#include "ble_report_receiver.h"' in main
    assert '#include "usb_keyboard_output.h"' in main
    assert "BleReportReceiver bleReceiver;" in main
    assert "UsbKeyboardOutput usbOutput;" in main
    assert "KEY_RELEASE_TIMEOUT_MS" in main
    assert "releaseAll" in main
    assert "USB_KEYBOARD_OUTPUT_ENABLED" in main


def test_no_legacy_network_or_modbus_dependencies_in_active_firmware():
    platformio = read("platformio.ini")
    main = read("src/main.cpp")

    assert "PubSubClient" not in platformio
    assert "modbus-esp8266" not in platformio
    assert "#include <WiFi" not in main
    assert "Serial1.begin" not in main
    assert "MQTT" not in main
