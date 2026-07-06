#include "ble_report_receiver.h"

#include <NimBLEDevice.h>
#include "config.h"

static BleReportReceiver* activeReceiver = nullptr;

#if BRIDGE_DEBUG_LOG
static void printReportHex(const uint8_t* data, size_t length) {
  Serial.print("[ble] report bytes=");
  for (size_t i = 0; i < length; ++i) {
    if (i > 0) {
      Serial.print(' ');
    }
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
  Serial.println();
}
#endif

class BridgeServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* server) override {
    (void)server;
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(true);
    }
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] connected");
#endif
  }

  void onDisconnect(NimBLEServer* server) override {
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(false);
    }
    NimBLEDevice::startAdvertising();
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] disconnected; advertising restarted");
#endif
  }
};

class ReportCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* characteristic) override {
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
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setDeviceName(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(64);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new BridgeServerCallbacks());

  NimBLEService* service = server->createService(BLE_KEYBOARD_SERVICE_UUID);
  NimBLECharacteristic* reportCharacteristic = service->createCharacteristic(
      BLE_KEYBOARD_REPORT_CHAR_UUID,
      NIMBLE_PROPERTY::WRITE);
  reportCharacteristic->setCallbacks(new ReportCharacteristicCallbacks());

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_KEYBOARD_SERVICE_UUID);
  advertising->setName(BLE_DEVICE_NAME);
  advertising->setAppearance(0x03C1);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
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
  printReportHex(data, length);
#endif
}
