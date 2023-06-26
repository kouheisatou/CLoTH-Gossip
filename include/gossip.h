#include "network.h"

struct channel_update {
  long channel_id;
  uint64_t timestamp;
  direction direction;
  uint32_t timelock;
  uint64_t min_htlc;
  uint64_t fee_base;
  uint64_t fee_proportional;
  uint64_t max_htlc;
};
