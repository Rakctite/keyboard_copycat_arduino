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
