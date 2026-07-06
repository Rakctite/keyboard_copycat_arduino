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
  friend class BridgeClientCallbacks;
  friend class ReportCharacteristicCallbacks;
  friend void receiveActiveBleReport(const uint8_t* data, size_t length);

  void setConnected(bool connected);
  void receiveReport(const uint8_t* data, size_t length);

  volatile bool connected_ = false;
  volatile bool reportReady_ = false;
  volatile bool disconnectReleaseNeeded_ = false;
  uint8_t latestReport_[KEYBOARD_REPORT_SIZE] = {0};
  uint32_t lastReportMillis_ = 0;
};
