#pragma once

#include "Ds1302Clock.h"
#include "FlashRing.h"

// Serial maintenance console. Reachable only for a short window after a cold
// boot (the node sleeps ~98 % of the time); see kMaintenanceWindowMs.
namespace Console {

// Waits `window_ms` for any byte; if one arrives, runs the console until
// `exit`. Returns true when the console was used.
bool maybeRun(uint32_t window_ms, Ds1302Clock& clock, FlashRing& ring);

}  // namespace Console
