#include "ble_report_receiver.h"

#include <NimBLEDevice.h>
#include "config.h"

static BleReportReceiver* activeReceiver = nullptr;
static NimBLEClient* client = nullptr;
static uint32_t lastConnectAttemptMs = 0;
static const uint32_t CONNECT_RETRY_INTERVAL_MS = 2000;

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

void receiveActiveBleReport(const uint8_t* data, size_t length) {
  if (activeReceiver != nullptr) {
    activeReceiver->receiveReport(data, length);
  }
}

static void notifyCallback(
    NimBLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify) {
  (void)characteristic;
  (void)isNotify;
  receiveActiveBleReport(data, length);
}

class BridgeClientCallbacks : public NimBLEClientCallbacks {
 public:
  void onConnect(NimBLEClient* connectedClient) override {
    (void)connectedClient;
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(true);
    }
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] connected to Windows sender");
#endif
  }

  void onDisconnect(NimBLEClient* disconnectedClient) override {
    (void)disconnectedClient;
    if (activeReceiver != nullptr) {
      activeReceiver->setConnected(false);
    }
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] disconnected from Windows sender");
#endif
  }
};

static NimBLEAdvertisedDevice* findKeyboardBridge() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->setMaxResults(10);

#if BRIDGE_DEBUG_LOG
  Serial.println("[ble] scanning for Windows sender");
#endif

  NimBLEScanResults results = scan->start(3, false);
  for (int i = 0; i < results.getCount(); ++i) {
    NimBLEAdvertisedDevice device = results.getDevice(i);
    const bool serviceMatch = device.isAdvertisingService(NimBLEUUID(BLE_KEYBOARD_SERVICE_UUID));
    const bool nameMatch = device.getName() == BLE_DEVICE_NAME;
    if (serviceMatch || nameMatch) {
#if BRIDGE_DEBUG_LOG
      Serial.print("[ble] found sender name=");
      Serial.print(device.getName().c_str());
      Serial.print(" address=");
      Serial.println(device.getAddress().toString().c_str());
#endif
      return new NimBLEAdvertisedDevice(device);
    }
  }

  scan->clearResults();
  return nullptr;
}

static bool connectAndSubscribe(NimBLEAdvertisedDevice* advertisedDevice) {
  if (client == nullptr) {
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(new BridgeClientCallbacks(), true);
    client->setConnectTimeout(5);
    client->setConnectionParams(6, 12, 0, 100);
  }

  if (client->isConnected()) {
    return true;
  }

  if (!client->connect(advertisedDevice)) {
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] connect failed");
#endif
    return false;
  }

  NimBLERemoteService* service = client->getService(BLE_KEYBOARD_SERVICE_UUID);
  if (service == nullptr) {
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] service not found on Windows sender");
#endif
    client->disconnect();
    return false;
  }

  NimBLERemoteCharacteristic* characteristic =
      service->getCharacteristic(BLE_KEYBOARD_REPORT_CHAR_UUID);
  if (characteristic == nullptr) {
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] report characteristic not found on Windows sender");
#endif
    client->disconnect();
    return false;
  }

  if (!characteristic->subscribe(true, notifyCallback, true)) {
#if BRIDGE_DEBUG_LOG
    Serial.println("[ble] subscribe failed");
#endif
    client->disconnect();
    return false;
  }

#if BRIDGE_DEBUG_LOG
  Serial.println("[ble] subscribed to keyboard reports");
#endif
  return true;
}

void BleReportReceiver::begin() {
  activeReceiver = this;
  NimBLEDevice::init("KeyboardBridgeArduino");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(64);
#if BRIDGE_DEBUG_LOG
  Serial.println("[ble] client mode ready");
#endif
}

bool BleReportReceiver::takeReport(uint8_t report[KEYBOARD_REPORT_SIZE]) {
  if (client == nullptr || !client->isConnected()) {
    const uint32_t now = millis();
    if (now - lastConnectAttemptMs >= CONNECT_RETRY_INTERVAL_MS) {
      lastConnectAttemptMs = now;
      NimBLEAdvertisedDevice* device = findKeyboardBridge();
      if (device != nullptr) {
        connectAndSubscribe(device);
        delete device;
      }
    }
  }

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
