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
