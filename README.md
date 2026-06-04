# negative-space-core-c

C implementation of the core **Negative Space Intelligence Theory** — the idea that intelligence is revealed not by what an entity chooses, but by what it avoids.

## Components

| Component | Description |
|---|---|
| **AvoidanceTracker** | Record actions and compute avoid/choose/unknown ratios and standard deviation |
| **ConservationLaw** | Verify avoidance ratio conservation across multiple temporal scales |
| **InferenceEngine** | Find gaps between avoidance regions and deduce hidden knowledge |
| **FeedbackLoop** | Update avoidance models with outcome data, trigger forced exploration |
| **BatchAnalyzer** | Analyze batches of decisions with entropy, avoid:choose ratios, and confidence |

## Build & Test

```bash
gcc -o test_core tests/test_core.c src/negative_space_core.c -lm -Wall -O2
./test_core
```

## Usage

```c
#include "src/negative_space_core.h"

NSAvoidanceTracker tracker;
ns_tracker_init(&tracker);
ns_tracker_record(&tracker, 1, NS_ACTION_AVOID, 0.95, 1.0);
ns_tracker_record(&tracker, 2, NS_ACTION_CHOOSE, 0.8, 2.0);

double avoid_pct = ns_tracker_avoid_ratio(&tracker);
```

## License

MIT
