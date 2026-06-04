# Future Integration: negative-space-core-c

## Current State
C implementation of Negative Space Intelligence Theory — intelligence is revealed by what an entity avoids, not what it chooses. Implements the core 5 laws with avoidance detection and analysis.

## Integration Opportunities

### With ternary-inference-c
The core defines what negative space IS; inference deduces knowledge from negative spaces. Together: core detects the gaps, inference fills them. On ESP32, this runs as a lightweight reasoning engine — deduce room state from what agents avoid doing.

### With compiled-policy-c
Negative space intelligence IS policy: "avoid these states." The C core defines the avoidance patterns; compiled-policy-c deploys them. A policy compiled from negative space theory says: if all agents avoid action X, there is probably a reason — preserve that avoidance.

### With ternary-cell (Avoidance-Aware Cells)
Cells that understand negative space make better predictions. A cell observes that its neighbors avoid state -1; it infers that -1 is undesirable without ever experiencing it. This is social learning through negative space — cells learn from what others avoid.

## Potential in Mature Systems
In room-as-codespace, negative space is the intelligence layer. A room where agents consistently avoid certain states has learned something. The C port captures that learning on edge devices. When a new agent enters the room, it downloads the room's negative space profile — what to avoid, learned by generations of previous agents.

## Cross-Pollination Ideas
- Cross-language negative space profiles: Rust generates, C consumes on edge
- Avoidance patterns as room signatures — fingerprint a room by what its agents avoid
- Negative space transfer between rooms: if room A avoids X, and room B is similar, B probably should too

## Dependencies for Next Steps
- Integration with ternary-inference-c for reasoning pipeline
- FFI bindings for Rust interop
- Room profile serialization format for negative space transfer
