// SPDX-License-Identifier: GPL-3.0-only
#include "app/runtime.h"
void setup() {
  Serial.begin(115200);
  if (!gw::startRuntime()) {
    Serial.println("OUTPUT: Runtime allocation failed; radio stopped");
    return;
  }
  gw::startWeb();
}
void loop() {
  gw::serviceWeb();
  delay(5);
}
