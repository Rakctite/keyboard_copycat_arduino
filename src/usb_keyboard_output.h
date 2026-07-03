#pragma once

#include <Arduino.h>
#include "keyboard_report.h"

class UsbKeyboardOutput {
 public:
  void begin();
  void sendReport(const uint8_t report[KEYBOARD_REPORT_SIZE]);
  void releaseAll();
};
