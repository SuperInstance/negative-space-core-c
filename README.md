# Negative Space Core (C)

**Negative Space Intelligence Theory** posits that intelligence is revealed not by what an entity chooses, but by what it avoids. This C library implements the four core primitives: avoidance tracking, conservation-law verification, knowledge inference from gaps, and closed-loop feedback correction.

## Why It Matters

Most intelligence models focus on positive signals — what was chosen, clicked, said. Negative Space flips this: the *absence* of action is itself a signal. In predator-prey ecology, animals that avoid certain regions reveal knowledge about resource distribution or threat presence. In markets, order-flow gaps reveal institutional positioning. In machine learning, a model's systematic misclassifications carve out the boundary of its competence. This library provides the computational primitives to operationalize that insight — tracking what wasn't done, checking whether the avoidance pattern is scale-invariant (a conservation law), inferring latent knowledge from the shape of gaps, and closing the loop with prediction-error feedback.

## How It Works

### Avoidance Tracking

The `NSAvoidanceTracker` records a timestamped stream of ternary actions: `AVOID (-1)`, `CHOOSE (+1)`, or `UNKNOWN (0)`. At any point, it computes the avoidance ratio:

```
R_avoid = N_avoid / N_total
```

along with the choose ratio and unknown ratio. The standard deviation of action types provides a dispersion measure. Storage is O(N) with a fixed-capacity ring of `NS_TRACKER_MAX_ACTIONS = 4096`.

### Conservation Law

The central hypothesis: *if avoidance ratios are conserved across scales, the agent possesses genuine knowledge*. The library partitions the action stream into `k` equal windows and computes the avoidance ratio for each:

```
R_k = N_avoid(window_k) / N_actions(window_k)
```

If the variance `Var(R_1, ..., R_k)` falls below a user-supplied threshold, the avoidance pattern is **conserved** — indicating structured knowledge rather than random noise. This is analogous to the Reynolds number in fluid dynamics: when the dimensionless ratio stays constant across scales, you have a physical law, not coincidence.

### Inference Engine

Given an avoidance map, the inference engine identifies **gaps** — contiguous regions of the action space that were systematically avoided. Each gap yields a `NSGap` struct with:

```
knowledge_estimate = f(gap_width, gap_density)
confidence = 1 - e^(-λ · gap_width)
```

Wider, denser gaps imply stronger latent knowledge. The total inferred knowledge aggregates across all gaps.

### Feedback Loop

The `NSFeedbackLoop` closes the cycle: for each action, compare the predicted avoidance probability against the actual outcome. The error signal is:

```
error_i = actual_i - predicted_i
RMSE = sqrt(Σ error_i² / N)
```

When RMSE exceeds a threshold, the system flags the need for exploration — the model's predictions are diverging from reality, indicating either environmental change or model misspecification.

## Quick Start

```c
#include "negative_space_core.h"

int main(void) {
    NSAvoidanceTracker tracker;
    ns_tracker_init(&tracker);

    // Record actions: agent avoids regions 1-3, chooses 4, avoids 5-7
    ns_tracker_record(&tracker, 1, NS_ACTION_AVOID, 0.9, 0.0);
    ns_tracker_record(&tracker, 2, NS_ACTION_AVOID, 0.85, 1.0);
    ns_tracker_record(&tracker, 3, NS_ACTION_AVOID, 0.8, 2.0);
    ns_tracker_record(&tracker, 4, NS_ACTION_CHOOSE, 0.7, 3.0);
    ns_tracker_record(&tracker, 5, NS_ACTION_AVOID, 0.9, 4.0);

    double avoid_ratio = ns_tracker_avoid_ratio(&tracker);
    printf("Avoid ratio: %.3f\n", avoid_ratio);

    // Check conservation across 3 scales
    NSConservationResult cr = ns_conservation_check(&tracker, 3, 0.05);
    printf("Conserved: %s (variance: %.6f)\n",
           cr.conserved ? "yes" : "no", cr.variance);

    // Infer knowledge from gaps
    NSInferenceResult ir = ns_inference_find_gaps(&tracker, 2.0);
    printf("Gaps found: %d, total inferred knowledge: %.3f\n",
           ir.num_gaps, ir.total_inferred_knowledge);

    return 0;
}
```

Compile with: `gcc -lm -o demo src/negative_space_core.c src/negative_space.c`

## API

| Component | Key Functions | Complexity |
|-----------|--------------|------------|
| AvoidanceTracker | `ns_tracker_init`, `ns_tracker_record`, `ns_tracker_avoid_ratio` | O(1) insert, O(N) ratios |
| ConservationLaw | `ns_conservation_check` | O(N + k) for k scales |
| InferenceEngine | `ns_inference_find_gaps` | O(N) linear scan |
| FeedbackLoop | `ns_feedback_record`, `ns_feedback_update`, `ns_feedback_should_explore` | O(1) per entry |
| BatchAnalyzer | `ns_batch_analyze` | O(N · B) for B batches |

## Architecture Notes

Negative Space Core implements the **η (eta) half** of SuperInstance's γ + η = C equation. Where γ (gamma) represents the generative/constructive drive, η captures the subtractive/avoidant intelligence. Together they yield C (competence). The conservation law is the bridge: if avoidance ratios are scale-invariant, the agent's competence is genuine rather than coincidental. See [ARCHITECTURE.md](https://github.com/SuperInstance/SuperInstance/blob/main/ARCHITECTURE.md) for the full γ + η = C framework.

## References

1. Craik, F. I. M., & Lockhart, R. S. (1972). "Levels of processing: A framework for memory research." *Journal of Verbal Learning and Verbal Behavior*, 11(6), 671–684.
2. Gibson, J. J. (1979). *The Ecological Approach to Visual Perception*. Houghton Mifflin. — Affordance theory: what the environment *offers* for action, including what to avoid.
3. Stewart, I. (1998). *Nature's Numbers: The Unreal Reality of Mathematics*. Basic Books. — On scale-invariant conservation laws in natural systems.

## License

MIT
