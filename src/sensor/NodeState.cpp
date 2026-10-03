#include "NodeState.h"

#include <string.h>

RTC_DATA_ATTR NodeState gState;

bool nodeStateInit() {
  if (gState.magic == kNodeStateMagic) return true;
  memset(&gState, 0, sizeof(gState));
  gState.magic = kNodeStateMagic;
  return false;
}
