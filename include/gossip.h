#include "network.h"

struct channel_update {
  long channel_id;

  uint64_t timestamp;

  /***
   * payment direction
   *
   * node1 -> node2 : 0
   * node2 -> node1 : 1
   */
  short direction;

  /***
   * amount of millisatoshi on sending payment
   */
  uint64_t htlc_maximum_msat;
};
