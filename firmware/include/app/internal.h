// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "runtime.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace gw {
struct StorageJob {
  uint8_t kind = 0, index = 0;
  uint16_t network = 0, node = 0;
  uint64_t session = 0, received = 0;
  uint32_t token = 0, correlation = 0;
  uint8_t bytes[446]{};
};
struct Completion {
  uint8_t kind = 0, index = 0;
  uint64_t session = 0;
  uint32_t token = 0, sequence = 0, correlation = 0;
  bool ok = false;
};
struct StorageView {
  bool healthy = false, sequences = false, fsMounted = false, archiveHealthy = false;
  uint32_t flashUsed = 0, flashTotal = 0;
  uint32_t stackMin = 0;
  size_t archiveCount = 0, sessionSlots = 0;
  ArchiveInfo archives[64]{};
};
extern QueueHandle_t commandQ, storageQ, completionQ, configQ, radioCommandQ, viewQ, resultQ,
    storageViewQ;
extern uint64_t bootId;
extern QueueHandle_t summaryQ;
struct ArchiveReply {
  bool ok = false;
  uint8_t index = 0;
  uint8_t bytes[ArchiveBytes]{};
};
extern QueueHandle_t archiveReplyQ;
void storageTask(void *);
void radioTask(void *);
} // namespace gw
