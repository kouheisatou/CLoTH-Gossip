# CLoTH-Gossip: Lightning Network Simulator - Technical Analysis

## Overview
CLoTH-Gossip is a Lightning Network payment channel simulator based on lnd-v0.10.0-beta. The core implementation reproduces HTLC (Hash Time Locked Contract) mechanics with high fidelity.

## Build Instructions
```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake ../.
make
```

## HTLC State Machine Architecture

### Event Types (States)
```c
enum event_type {
  FINDPATH,              // Path finding
  SENDPAYMENT,           // Sender initiates HTLC
  FORWARDPAYMENT,        // Intermediate hop forwards HTLC
  RECEIVEPAYMENT,        // Receiver accepts payment
  FORWARDSUCCESS,        // Forward success back
  RECEIVESUCCESS,        // Sender receives success
  FORWARDFAIL,           // Forward failure back
  RECEIVEFAIL,           // Sender receives failure
  CHANNELUPDATEFAIL,     // Channel update on failure
  CHANNELUPDATESUCCESS,  // Channel update on success
  UPDATEGROUP,           // Group capacity update
  CONSTRUCTGROUPS,       // Group construction
}
```

### State Transition Diagram (Original CLoTH)

The base state machine design from the CLoTH paper (Figure 2):

```
                    ┌──────────────┐
              ┌────►│  find path   │◄────┐
              │     └──────┬───────┘     │
              │            │             │
              │            │ path found  │
              │            ▼             │
              │     ┌──────────────┐     │
              │     │    send      │     │
              │     │   payment    │     │
              │     └──────┬───────┘     │
              │            │             │
              │            │ forward     │
              │            ▼             │
              │     ┌──────────────┐     │
              │     │   forward    │◄───┐│
              │     │   payment    │    ││ (loop per hop)
              │     └──────┬───────┘    ││
              │            │            ││
              │            ├────────────┘│
              │            │             │
              │            │ last hop    │
              │            ▼             │
              │     ┌──────────────┐     │
              │     │   receive    │     │
              │     │   payment    │     │
              │     └──────┬───────┘     │
              │            │             │
              │            │ success     │
              │            ▼             │
              │     ┌──────────────┐     │
   fail       │     │   forward    │◄───┐│
    ┌─────────┤     │   success    │    ││ (loop per hop)
    │         │     └──────┬───────┘    ││
    │         │            │            ││
    │         │            ├────────────┘│
    │         │            │             │
    │         │            │ first hop   │
    │         │            ▼             │
    │         │     ┌──────────────┐     │
    │         └─────┤   receive    │     │
    │               │   success    │─────┘
    │               └──────────────┘
    │                                retry
    │
    │  ┌──────────────┐
    └─►│   forward    │◄───┐
       │     fail     │    │ (loop per hop)
       └──────┬───────┘    │
              │            │
              ├────────────┘
              │
              │ first hop
              ▼
       ┌──────────────┐
       │   receive    │
       │     fail     │─────┐
       └──────────────┘     │
                            │ retry
                            └─► (back to find path)
```

### Enhanced State Diagram with MPP (Proposed)

Adding recursive multi-path payment splitting:

```
                    ┌──────────────┐
              ┌────►│  find path   │◄────────────────┐
              │     └──────┬───────┘                 │
              │            │                         │ retry
              │            │ path found              │
              │            ▼                         │
              │     ┌──────────────┐                 │
              │     │    send      │                 │
              │     │   payment    │                 │
              │     └──────┬───────┘                 │
              │            │                         │
              │            │ forward                 │
              │            ▼                         │
              │     ┌──────────────┐                 │
              │     │   forward    │◄───┐            │
              │     │   payment    │    │ (loop)     │
              │     └──────┬───────┘    │            │
              │            │            │            │
              │            ├────────────┘            │
              │            │                         │
              │            │ last hop                │
              │            ▼                         │
              │     ┌──────────────┐                 │
              │     │   receive    │                 │
              │     │   payment    │                 │
              │     └──────┬───────┘                 │
              │            │                         │
              │            │ success                 │
              │            ▼                         │
              │     ┌──────────────┐                 │
   fail       │     │   forward    │◄───┐            │
    ┌─────────┤     │   success    │    │ (loop)     │
    │         │     └──────┬───────┘    │            │
    │         │            │            │            │
    │         │            ├────────────┘            │
    │         │            │                         │
    │         │            │ first hop               │
    │         │            ▼                         │
    │         │     ┌──────────────┐                 │
    │         └─────┤   receive    │                 │
    │               │   success    │◄────┐           │
    │               └──────┬───────┘     │           │
    │                      │             │           │
    │                      │ if child    │ notify    │
    │                      │   shard     │ parent    │
    │                      │             │           │
    │                      ▼             │           │
    │               ┌──────────────┐     │           │
    │               │ Update Parent│─────┘           │
    │               │ (succeeded++) │                │
    │               └──────┬───────┘                 │
    │                      │                         │
    │                      │ if all shards done      │
    │                      │ & all succeeded         │
    │                      │    → parent success     │
    │                      └─────────────────────────┘
    │
    │  ┌──────────────┐
    └─►│   forward    │◄───┐
       │     fail     │    │ (loop per hop)
       └──────┬───────┘    │
              │            │
              ├────────────┘
              │
              │ first hop
              ▼
       ┌──────────────┐
       │   receive    │
       │     fail     │
       └──────┬───────┘
              │
              │ Can split?
              ├──────────────────────┐
              │ YES                  │ NO
              │                      │
              ▼                      │
       ┌──────────────┐              │
       │ Create Child │              │ if child shard
       │   Shards     │              │    → notify parent
       │ (amount/2)   │              │
       └──────┬───────┘              │
              │                      │
              │ Emit FINDPATH        │
              │ for each child       │
              │                      ▼
              │               ┌──────────────┐
              │               │ Update Parent│◄──┐
              │               │ (failed++)   │   │
              │               └──────┬───────┘   │
              │                      │           │
              │                      │ if all    │ notify
              │                      │ shards    │ parent
              │                      │ done      │
              │                      │           │
              └──────────────────────┴───────────┘
                 (child shards follow same flow)
```

**Key MPP Enhancements:**
1. **Recursive splitting**: `receive fail` can split into 2 child shards
2. **Parent notification**: Child shards notify parent on success/failure via `receive success`/`receive fail`
3. **All-or-nothing**: Parent succeeds only when all child shards succeed
4. **Event-driven**: No polling, pure event notifications through existing states

### Detailed State Transition Flow

```
Original Flow (Single Payment):
  find path → send payment → forward payment (N hops) → receive payment
    → forward success (N hops) → receive success ✓

MPP Flow (Parent Payment Fails):
  find path → send payment → ... → receive fail
    → split into child1(amount/2), child2(amount/2)
    → emit FINDPATH(child1), FINDPATH(child2)
    → parent waits (num_shards_pending=2)

MPP Flow (Child Succeeds):
  child1: ... → receive success
    → notify parent: parent.num_shards_succeeded++
    → if all done & all succeeded → parent.is_success = 1 ✓

MPP Flow (Child Fails, Can Split):
  child1: ... → receive fail
    → split into child1a(amount/4), child1b(amount/4)
    → emit FINDPATH(child1a), FINDPATH(child1b)
    → parent.num_shards_pending = 3 (child2, child1a, child1b)

MPP Flow (Child Fails, Cannot Split):
  child1a: ... → receive fail (amount < min_shard_amt)
    → notify parent: parent.num_shards_failed++
    → if all done → parent.is_success = 0 ✗
```

## Key Processing Flows

### 1. Success Path
```
FINDPATH → SENDPAYMENT → FORWARDPAYMENT* → RECEIVEPAYMENT 
         → FORWARDSUCCESS* → RECEIVESUCCESS → CHANNELUPDATESUCCESS
```

### 2. Failure Path
```
Any state → FORWARDFAIL* → RECEIVEFAIL → FINDPATH (retry)
                                       → CHANNELUPDATEFAIL
```

### 3. Error Types
- **OFFLINENODE**: Next node is offline (3 second timeout: `OFFLINELATENCY`)
- **NOBALANCE**: Insufficient balance or policy violation

## Balance Management

### Forward Phase (Locking)
```c
// send_payment() or forward_payment()
next_edge->balance -= hop->amount_to_forward;  // Lock funds
```

### Success Phase (Settlement)
```c
// receive_payment() or forward_success()
backward_edge->balance += hop->amount_to_forward;  // Complete transfer
```

### Failure Phase (Rollback)
```c
// forward_fail() or receive_fail()
next_edge->balance += hop->amount_to_forward;  // Restore balance
```

## Policy Validation

Each hop validates:
1. **Sufficient balance**: `amount_to_forward <= edge->balance`
2. **Minimum HTLC**: `amount_to_forward >= edge->policy.min_htlc`
3. **Fee correctness**: `prev_amount = next_amount + computed_fee`
4. **Timelock**: `prev_timelock = next_timelock + edge->policy.timelock`

```c
// check_balance_and_policy() in htlc.c
uint64_t expected_fee = compute_fee(next_hop->amount_to_forward, edge->policy);
if(prev_hop->amount_to_forward != next_hop->amount_to_forward + expected_fee) {
    // Policy violation
}
```

## Multi-Path Payments (MPP)

### Current Implementation (Limited)

If single path fails on **first attempt only** and `mpp=1`:
```c
// find_path() splits payment into two shards (once only)
if(mpp && path == NULL && !(payment->is_shard) && payment->attempts == 1) {
    shard1_amount = payment->amount / 2;
    shard2_amount = payment->amount - shard1_amount;
    // Each shard follows independent HTLC flow
    // Shards are NOT further split if they fail
}
```

**Limitations:**
- Only splits once (on first failure)
- Fixed 2-shard split
- Shards cannot be further divided
- Parent payment fails if either shard fails

### LND Implementation (Recursive Splitting)

LND's actual MPP uses recursive splitting in `routing/payment_session.go`:

```go
// RequestRoute() loops and splits recursively
for {
    path = findPath(maxAmt, ...)
    
    if err == errNoPathFound {
        // Check limits
        if activeShards+1 >= p.payment.MaxParts { return err }
        if maxAmt < p.minShardAmt { return err }
        
        // Split and retry
        maxAmt /= 2  // Recursive halving
        continue     // Try again with smaller amount
    }
    return route
}
```

**Advantages:**
- Recursive splitting until min shard size or max shards
- Each attempt can trigger further splits
- Realistic failure-driven adaptation
- Better success rate for large payments

## Node Pair Results (Mission Control)

Tracks success/failure history per node pair for routing optimization:

```c
struct node_pair_result {
    long to_node_id;
    uint64_t fail_time;      // When failure occurred
    uint64_t fail_amount;    // Amount that failed
    uint64_t success_time;   // When success occurred
    uint64_t success_amount; // Amount that succeeded
};
```

Updated on:
- **Success**: `set_node_pair_result_success()` - increases success_amount
- **Failure**: `set_node_pair_result_fail()` - sets fail_amount/time

## Group Management (CLoTH Extension)

### Group Update Event
```c
// receive_success() triggers:
UPDATEGROUP → request_group_update() → update_group()
           → if group closes → CONSTRUCTGROUPS
```

### Group Construction
- Groups edges with similar capacities
- Size: `net_params.group_size`
- Capacity limits: `±group_limit_rate` from requesting edge
- Prevents loops: edges cannot share endpoints with group members

### Routing Methods
1. **CLOTH_ORIGINAL**: Uses pre-computed paths
2. **GROUP_ROUTING**: Capacity-based grouping with limit rate
3. **GROUP_ROUTING_CUL**: Capacity uncertainty limit based grouping

## Timing Parameters

```c
// Default latencies
OFFLINELATENCY = 3000ms  // TCP retransmission timeout

// Per-hop forwarding delay (Gaussian distribution)
next_event_time = current_time 
                + average_payment_forward_interval 
                + variance * gsl_ran_ugaussian()
```

## File Structure

### Core HTLC Implementation
- **src/htlc.c**: All HTLC state machine functions (938 lines)
- **include/htlc.h**: HTLC function declarations
- **include/event.h**: Event type definitions
- **include/payments.h**: Payment structure and error types

### Supporting Modules
- **src/routing.c**: Dijkstra pathfinding with capacity estimation
- **src/network.c**: Network topology and edge management
- **src/event.c**: Event queue management (heap-based priority queue)

## Payment Lifecycle Example

```
1. FINDPATH (sender)
   - Dijkstra search with capacity constraints
   - Transform path to route with fees/timelocks
   
2. SENDPAYMENT (sender)
   - Lock balance on first edge
   - Check if next node is online
   - Schedule FORWARDPAYMENT event
   
3. FORWARDPAYMENT (each intermediate hop)
   - Validate balance and policies
   - Lock balance on next edge
   - Schedule next FORWARDPAYMENT or RECEIVEPAYMENT
   
4. RECEIVEPAYMENT (receiver)
   - Update counter-edge balance (settlement)
   - Schedule FORWARDSUCCESS
   
5. FORWARDSUCCESS (backward through hops)
   - Update counter-edge balances
   - Propagate success backward
   
6. RECEIVESUCCESS (sender)
   - Record success in node_pair_results
   - Trigger channel update and group update events
```

## Retry Mechanism

### Original (Pre-MPP)
On `RECEIVEFAIL`:
```c
// receive_fail() in htlc.c
1. Record channel_update with error edge
2. Add attempt to payment history
3. Restore locked balance
4. Schedule FINDPATH event (retry)
   - Dijkstra excludes failed edges from history
5. Check payment_timeout
   - If exceeded: mark is_timeout=1, abort
```

### Enhanced with MPP

**Two splitting triggers:**

1. **In find_path() - No route found**:
   - Dijkstra returns NULL (no path exists)
   - Split into 2 shards with half the amount
   - Emit FINDPATH for both child shards
   - This handles "amount too large for any single path" cases

2. **In receive_fail() - Payment transmission failed**:
   - Payment was sent but failed (NOBALANCE, OFFLINENODE, etc.)
   - Only NOBALANCE errors trigger splitting
   - Split into 2 shards with half the amount
   - Record split in attempt history
   - Emit FINDPATH for both child shards
   - This handles "path exists but insufficient capacity" cases
   - If exceeded: mark is_timeout=1, abort
```

## Key Insights

1. **State Machine Design**: CLoTH uses 8 core states (find path, send payment, forward payment, receive payment, forward success, receive success, forward fail, receive fail) with loop structures for multi-hop forwarding
2. **Strict Forwarding**: Current implementation uses strict forwarding (exact edge specified in route)
3. **Balance Consistency**: All balance changes are symmetric - forward locks, backward settles or rollbacks
4. **Event-Driven Architecture**: Entire simulation is event-driven with heap-based priority queue sorted by time
5. **LND Fidelity**: Code closely mirrors lnd functions (routing/missioncontrol.go, htlcswitch/link.go)
6. **Statistics Tracking**: Counts offline_node_count, no_balance_count, attempts per payment
7. **MPP Limitation**: Current MPP only splits once on first failure; LND uses recursive splitting

## Proposed MPP Enhancement

To match LND's behavior while preserving the event-driven state machine:

### Strategy: Event-Driven Recursive Splitting with Parent Notification

**Core Principle**: All-or-nothing success. Parent succeeds only when ALL shards succeed.

1. **On FINDPATH failure (no route)**: Check if payment can be split
2. **On child RECEIVEFAIL**: Check if payment can be split, OR notify parent of failure
3. **On child RECEIVESUCCESS**: Notify parent of success
4. **Parent decides**: When notified by children, parent checks if all shards succeeded
5. **No polling**: Pure event-driven via RECEIVESUCCESS/RECEIVEFAIL

**Important**: MPP splitting occurs in TWO places:
- **find_path()**: When no route is found (before attempting payment)
- **receive_fail()**: When payment fails during transmission (NOBALANCE error)

### Key Design Points

```c
struct payment {
    uint64_t amount;           // Target amount (parent) or shard amount
    uint64_t amount_to_send;   // Current attempt amount
    long parent_id;            // -1 for parent, else parent payment ID
    
    // Parent-only fields
    struct element* child_shards;  // List of child payment IDs
    uint32_t num_shards_pending;   // Shards still in-flight
    uint32_t num_shards_succeeded; // Shards that succeeded
    uint32_t num_shards_failed;    // Shards that failed
    uint32_t total_shards_created; // Total shards created
    // ...
};

// === CHILD SHARD: On RECEIVEFAIL ===
if (payment->parent_id == -1) {
    // This is parent's own attempt failing
    
    if (can_split(payment, net_params)) {
        // Split into 2 child shards
        uint64_t new_amt = payment->amount / 2;
        
        shard1 = create_shard(new_amt, payment->id, ...);
        shard2 = create_shard(new_amt, payment->id, ...);
        
        payment->child_shards = push(payment->child_shards, shard1->id);
        payment->child_shards = push(payment->child_shards, shard2->id);
        payment->num_shards_pending = 2;
        payment->total_shards_created = 2;
        
        // Emit FINDPATH events for children
        emit_event(FINDPATH, shard1);
        emit_event(FINDPATH, shard2);
    } else {
        // Cannot split -> complete failure
        payment->is_success = 0;
        payment->end_time = current_time;
    }
} else {
    // This is a child shard failing
    parent = get_payment(payment->parent_id);
    
    if (can_split(payment, net_params)) {
        // Split this child into 2 smaller shards
        uint64_t new_amt = payment->amount / 2;
        
        child1 = create_shard(new_amt, parent->id, ...);
        child2 = create_shard(new_amt, parent->id, ...);
        
        parent->child_shards = push(parent->child_shards, child1->id);
        parent->child_shards = push(parent->child_shards, child2->id);
        parent->num_shards_pending += 1;  // -1 (this) +2 (children) = +1
        parent->total_shards_created += 2;
        
        emit_event(FINDPATH, child1);
        emit_event(FINDPATH, child2);
    } else {
        // Cannot split -> notify parent of failure
        parent->num_shards_pending--;
        parent->num_shards_failed++;
        
        // Check if all shards finished
        if (parent->num_shards_pending == 0) {
            // All shards done -> parent fails (at least one failed)
            parent->is_success = 0;
            parent->end_time = current_time;
        }
    }
}

// === CHILD SHARD: On RECEIVESUCCESS ===
if (payment->parent_id != -1) {
    // Child succeeded -> notify parent
    parent = get_payment(payment->parent_id);
    parent->num_shards_pending--;
    parent->num_shards_succeeded++;
    
    // Check if all shards finished
    if (parent->num_shards_pending == 0) {
        if (parent->num_shards_failed == 0) {
            // ALL shards succeeded!
            parent->is_success = 1;
            parent->end_time = current_time;
        } else {
            // Some failed -> parent fails
            parent->is_success = 0;
            parent->end_time = current_time;
        }
    }
}
```

### MPP State Transitions

```
Parent Payment Fails (amount too large):
  RECEIVEFAIL (parent) 
    → split into shard1, shard2
    → emit FINDPATH(shard1), FINDPATH(shard2)
    → parent waits (num_shards_pending=2)

Shard1 Fails (still too large):
  RECEIVEFAIL (shard1)
    → split into shard1a, shard1b
    → parent.num_shards_pending = 3 (shard2, shard1a, shard1b)

Shard2 Succeeds:
  RECEIVESUCCESS (shard2)
    → parent.num_shards_succeeded++
    → parent.num_shards_pending = 2 (still waiting for shard1a, shard1b)

Shard1a Succeeds:
  RECEIVESUCCESS (shard1a)
    → parent.num_shards_pending = 1 (waiting for shard1b)

Shard1b Succeeds:
  RECEIVESUCCESS (shard1b)
    → parent.num_shards_pending = 0
    → parent.num_shards_failed = 0
    → parent.is_success = 1 ✓ COMPLETE SUCCESS!

Alternative: Any Shard Fails Completely:
  RECEIVEFAIL (shard1b, cannot split further)
    → parent.num_shards_failed++
    → parent.num_shards_pending = 0
    → parent.is_success = 0 ✗ COMPLETE FAILURE
```

**Benefits:**
- **Pure event-driven**: No polling, all via RECEIVESUCCESS/RECEIVEFAIL
- **All-or-nothing**: Parent succeeds only when all shards succeed
- **Clean state machine**: No changes to core states
- **Realistic**: Matches real LN behavior (try, fail, split, retry)
- **Flexible**: Recursive splitting up to min_shard_amt or max_shards

## Payment Output CSV Format

The `payments_output.csv` includes an `attempts_history` column with detailed JSON tracking of payment attempts and MPP splits:

```json
{
  "attempts": 1,
  "is_succeeded": 0,
  "end_time": 12345,
  "error_edge": 42,
  "error_type": 1,
  "is_split": 1,
  "split_amount": 50000,
  "child_shard1_id": 100,
  "child_shard2_id": 101,
  "route": [...]
}
```

**MPP Split Tracking Fields:**
- `is_split`: 1 if this attempt resulted in splitting into child shards, 0 otherwise
- `split_amount`: Amount of each child shard (original amount / 2)
- `child_shard1_id`: Payment ID of first child shard
- `child_shard2_id`: Payment ID of second child shard

**Example MPP Split Tree:**
```
Payment 0 (100 sat) fails
  → splits into shard1 (50 sat, id=10) and shard2 (50 sat, id=11)
    → shard1 (50 sat) fails
      → splits into shard1a (25 sat, id=20) and shard1b (25 sat, id=21)
        → shard1a (25 sat) succeeds ✓
        → shard1b (25 sat) succeeds ✓
    → shard2 (50 sat) succeeds ✓
  → Payment 0 succeeds (all children succeeded)
```

The attempts_history for payment 0 would show:
```json
[
  {"attempts":1, "is_succeeded":0, "is_split":1, "split_amount":50000, 
   "child_shard1_id":10, "child_shard2_id":11, ...}
]
```

The attempts_history for shard1 (id=10) would show:
```json
[
  {"attempts":1, "is_succeeded":0, "is_split":1, "split_amount":25000,
   "child_shard1_id":20, "child_shard2_id":21, ...}
]
```

The attempts_history for shard1a (id=20) and shard1b (id=21) would show:
```json
[
  {"attempts":1, "is_succeeded":1, "is_split":0, ...}
]
```

This allows complete reconstruction of the MPP split tree from the CSV output.

## References

Based on lnd-v0.10.0-beta implementation:
- `routing/missioncontrol.go`: Node pair results
- `htlcswitch/switch.go`: HTLC forwarding logic
- `htlcswitch/link.go`: Link-level HTLC handling
