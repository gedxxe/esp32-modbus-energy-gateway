// SPDX-License-Identifier: GPL-3.0-only
#include "internal.h"
#include <esp_timer.h>
#include <esp_system.h>
#include <freertos/semphr.h>
namespace gw {
QueueHandle_t commandQ = nullptr, storageQ = nullptr, completionQ = nullptr, configQ = nullptr,
              radioCommandQ = nullptr, viewQ = nullptr, resultQ = nullptr, storageViewQ = nullptr;
uint64_t bootId = 0;
QueueHandle_t archiveReplyQ = nullptr;
QueueHandle_t summaryQ = nullptr;
namespace {
SemaphoreHandle_t logMutex = nullptr;
char logs[32][112]{};
unsigned logHead = 0, logCount = 0;
} // namespace
uint64_t mono() {
  return uint64_t(esp_timer_get_time()) / 1000;
}
void logEvent(const char *text) {
  if (!logMutex || xSemaphoreTake(logMutex, pdMS_TO_TICKS(5)) != pdTRUE)
    return;
  snprintf(logs[logHead], 112, "%llu ms %s", (unsigned long long)mono(), text);
  logHead = (logHead + 1) % 32;
  if (logCount < 32)
    logCount++;
  xSemaphoreGive(logMutex);
}
String exportLog() {
  String s;
  s.reserve(4096);
  if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (unsigned i = 0; i < logCount; i++) {
      s += logs[(logHead + 32 - logCount + i) % 32];
      s += '\n';
    }
    xSemaphoreGive(logMutex);
  }
  return s;
}
bool startRuntime() {
  bootId = (uint64_t(esp_random()) << 32) | esp_random();
  if (!bootId)
    bootId = 1;
  logMutex = xSemaphoreCreateMutex();
  commandQ = xQueueCreate(4, sizeof(Command));
  storageQ = xQueueCreate(4, sizeof(StorageJob));
  completionQ = xQueueCreate(8, sizeof(Completion));
  configQ = xQueueCreate(1, sizeof(Config));
  radioCommandQ = xQueueCreate(4, sizeof(Command));
  viewQ = xQueueCreate(1, sizeof(View));
  resultQ = xQueueCreate(8, sizeof(Result));
  storageViewQ = xQueueCreate(1, sizeof(StorageView));
  archiveReplyQ = xQueueCreate(1, sizeof(ArchiveReply));
  summaryQ = xQueueCreate(1, sizeof(StatusSummary));
  if (!logMutex || !commandQ || !storageQ || !completionQ || !configQ || !radioCommandQ || !viewQ ||
      !resultQ || !storageViewQ || !archiveReplyQ || !summaryQ)
    return false;
  return xTaskCreate(storageTask, "storage-owner", 16384, nullptr, 1, nullptr) == pdPASS &&
         xTaskCreate(radioTask, "radio-owner", 24576, nullptr, 2, nullptr) == pdPASS;
}
bool submit(const Command &c) {
  return commandQ && xQueueSend(commandQ, &c, 0) == pdTRUE;
}
bool snapshot(View &v) {
  return viewQ && xQueuePeek(viewQ, &v, 0) == pdTRUE;
}
bool snapshotSummary(StatusSummary &s) {
  return summaryQ && xQueuePeek(summaryQ, &s, 0) == pdTRUE;
}
bool readArchive(uint8_t index, uint8_t *out) {
  ArchiveReply old;
  while (xQueueReceive(archiveReplyQ, &old, 0) == pdTRUE) {
  }
  StorageJob job;
  job.kind = 3;
  job.index = index;
  if (xQueueSend(storageQ, &job, 0) != pdTRUE)
    return false;
  ArchiveReply reply;
  if (xQueueReceive(archiveReplyQ, &reply, pdMS_TO_TICKS(500)) != pdTRUE || reply.index != index ||
      !reply.ok)
    return false;
  memcpy(out, reply.bytes, ArchiveBytes);
  return true;
}
} // namespace gw
