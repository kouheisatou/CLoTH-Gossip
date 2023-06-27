#include "network.h"

struct channel_update {
  long channel_id;
  long from_node_id;
  long to_node_id;
  uint64_t timestamp;
  uint32_t timelock;
  uint64_t min_htlc;
  uint64_t fee_base;
  uint64_t fee_proportional;
  uint64_t max_htlc;
};
