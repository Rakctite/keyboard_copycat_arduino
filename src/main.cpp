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
#if USB_KEYBOARD_OUTPUT_ENABLED
  usbOutput.releaseAll();
#else
  (void)reason;
#if BRIDGE_DEBUG_LOG
  Serial.println("[usb] output disabled; release skipped");
#endif
#endif
}

void setup() {
  Serial.begin(115200);
  delay(500);

#if USB_KEYBOARD_OUTPUT_ENABLED
  usbOutput.begin();
#else
#if BRIDGE_DEBUG_LOG
  Serial.println("[usb] output disabled by config");
#endif
#endif
  bleReceiver.begin();

  timeoutReleaseSent = true;
#if BRIDGE_DEBUG_LOG
  Serial.println("[bridge] ready");
#endif
}

void loop() {
  uint8_t report[KEYBOARD_REPORT_SIZE];

  if (bleReceiver.takeReport(report)) {
#if USB_KEYBOARD_OUTPUT_ENABLED
    usbOutput.sendReport(report);
#else
#if BRIDGE_DEBUG_LOG
    Serial.println("[usb] output disabled; report not sent");
#endif
#endif
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
