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
  KeyReport keyReport;
  keyReport.modifiers = report[0];
  keyReport.reserved = report[1];
  memcpy(keyReport.keys, report + 2, sizeof(keyReport.keys));
  Keyboard.sendReport(&keyReport);
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
