#include "network.h"

struct channel_update {
  uint64_t timestamp;

  struct edge* edge;

  /***
   * amount of millisatoshi on sending payment
   */
  uint64_t htlc_maximum_msat;
};
