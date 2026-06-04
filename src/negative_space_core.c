#include "negative_space_core.h"
#include <string.h>
#include <stdlib.h>

/* ── AvoidanceTracker ────────────────────────────────────────────────── */

void ns_tracker_init(NSAvoidanceTracker *t) {
    memset(t, 0, sizeof(*t));
}

int ns_tracker_record(NSAvoidanceTracker *t, int id, NSActionType action,
                      double confidence, double timestamp) {
    if (t->count >= NS_TRACKER_MAX_ACTIONS) return -1;
    t->actions[t->count].id         = id;
    t->actions[t->count].action     = action;
    t->actions[t->count].confidence = confidence;
    t->actions[t->count].timestamp  = timestamp;
    t->count++;
    return t->count - 1;
}

int ns_tracker_count_by_type(const NSAvoidanceTracker *t, NSActionType type) {
    int c = 0;
    for (int i = 0; i < t->count; i++)
        if (t->actions[i].action == type) c++;
    return c;
}

double ns_tracker_avoid_ratio(const NSAvoidanceTracker *t) {
    if (t->count == 0) return 0.0;
    return (double)ns_tracker_count_by_type(t, NS_ACTION_AVOID) / (double)t->count;
}

double ns_tracker_choose_ratio(const NSAvoidanceTracker *t) {
    if (t->count == 0) return 0.0;
    return (double)ns_tracker_count_by_type(t, NS_ACTION_CHOOSE) / (double)t->count;
}

double ns_tracker_unknown_ratio(const NSAvoidanceTracker *t) {
    if (t->count == 0) return 0.0;
    return (double)ns_tracker_count_by_type(t, NS_ACTION_UNKNOWN) / (double)t->count;
}

double ns_tracker_std(const NSAvoidanceTracker *t) {
    if (t->count == 0) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < t->count; i++)
        mean += t->actions[i].confidence;
    mean /= t->count;

    double var = 0.0;
    for (int i = 0; i < t->count; i++) {
        double d = t->actions[i].confidence - mean;
        var += d * d;
    }
    var /= t->count;
    return sqrt(var);
}

/* ── ConservationLaw ─────────────────────────────────────────────────── */

NSConservationResult ns_conservation_check(const NSAvoidanceTracker *t,
                                           int num_scales, double threshold) {
    NSConservationResult r;
    memset(&r, 0, sizeof(r));

    if (t->count < num_scales || num_scales <= 0 || num_scales > NS_MAX_SCALES)
        return r;

    int per_scale = t->count / num_scales;

    for (int s = 0; s < num_scales; s++) {
        int start = s * per_scale;
        int end   = (s == num_scales - 1) ? t->count : (s + 1) * per_scale;
        int avoids = 0;
        for (int i = start; i < end; i++)
            if (t->actions[i].action == NS_ACTION_AVOID) avoids++;
        r.scales[s].avoid_ratio     = (double)avoids / (double)(end - start);
        r.scales[s].n_actions       = end - start;
        r.scales[s].timestamp_start = t->actions[start].timestamp;
        r.scales[s].timestamp_end   = t->actions[end - 1].timestamp;
        r.num_scales++;
    }

    /* Compute variance of avoid ratios */
    double mean = 0.0;
    for (int i = 0; i < r.num_scales; i++)
        mean += r.scales[i].avoid_ratio;
    mean /= r.num_scales;

    double var = 0.0;
    for (int i = 0; i < r.num_scales; i++) {
        double d = r.scales[i].avoid_ratio - mean;
        var += d * d;
    }
    r.variance  = var / r.num_scales;
    r.conserved = (r.variance < threshold) ? 1 : 0;

    return r;
}

/* ── InferenceEngine ─────────────────────────────────────────────────── */

NSInferenceResult ns_inference_find_gaps(const NSAvoidanceTracker *t,
                                         double min_gap_width) {
    NSInferenceResult r;
    memset(&r, 0, sizeof(r));

    if (t->count < 2) return r;

    /* Find consecutive avoid regions, then detect gaps (choose/unknown)
     * between them as potential knowledge boundaries. */
    int in_avoid_region = 0;
    int last_avoid_end = -1;

    for (int i = 0; i < t->count; i++) {
        if (t->actions[i].action == NS_ACTION_AVOID) {
            if (!in_avoid_region) {
                /* entered avoid region */
                in_avoid_region = 1;
            }
        } else {
            if (in_avoid_region) {
                last_avoid_end = i - 1;
                in_avoid_region = 0;
            }
        }
        /* Check if we just left an avoid region and see a gap before the next */
        if (!in_avoid_region && last_avoid_end >= 0 && r.num_gaps < NS_MAX_GAPS) {
            /* Scan ahead for next avoid */
            int next_avoid = -1;
            for (int j = i; j < t->count; j++) {
                if (t->actions[j].action == NS_ACTION_AVOID) {
                    next_avoid = j;
                    break;
                }
            }
            if (next_avoid > 0) {
                double gap_width = t->actions[next_avoid - 1].timestamp -
                                   t->actions[last_avoid_end].timestamp;
                if (gap_width >= min_gap_width) {
                    NSGap *g = &r.gaps[r.num_gaps++];
                    g->gap_start_id       = last_avoid_end;
                    g->gap_end_id         = next_avoid;
                    /* Knowledge estimate: tighter gap → higher knowledge */
                    g->knowledge_estimate = 1.0 / (1.0 + gap_width);
                    /* Confidence based on action densities */
                    int gap_actions = next_avoid - last_avoid_end - 1;
                    g->confidence   = (gap_actions > 0) ?
                        (double)gap_actions / (double)t->count : 0.5;
                }
                last_avoid_end = -1; /* Only record gap once per pair */
            }
        }
    }

    /* Aggregate inferred knowledge */
    for (int i = 0; i < r.num_gaps; i++)
        r.total_inferred_knowledge += r.gaps[i].knowledge_estimate;
    if (r.num_gaps > 0)
        r.total_inferred_knowledge /= r.num_gaps;

    return r;
}

/* ── FeedbackLoop ────────────────────────────────────────────────────── */

void ns_feedback_init(NSFeedbackLoop *fl) {
    memset(fl, 0, sizeof(*fl));
    fl->model_adjustment = 1.0;
}

int ns_feedback_record(NSFeedbackLoop *fl, int action_id,
                       double predicted, double actual) {
    if (fl->count >= NS_TRACKER_MAX_ACTIONS) return -1;
    fl->entries[fl->count].action_id          = action_id;
    fl->entries[fl->count].predicted_avoid_prob = predicted;
    fl->entries[fl->count].actual_outcome      = actual;
    fl->entries[fl->count].error               = actual - predicted;
    fl->count++;
    return 0;
}

void ns_feedback_update(NSFeedbackLoop *fl) {
    if (fl->count == 0) return;

    double sum_err  = 0.0;
    double sum_sq   = 0.0;
    for (int i = 0; i < fl->count; i++) {
        sum_err += fl->entries[i].error;
        sum_sq  += fl->entries[i].error * fl->entries[i].error;
    }
    fl->mean_error = sum_err / fl->count;
    fl->rmse       = sqrt(sum_sq / fl->count);

    /* Adjust model: if we consistently overpredict avoidance (error > 0),
       reduce adjustment; if underpredict, increase. */
    fl->model_adjustment -= 0.1 * fl->mean_error;
    if (fl->model_adjustment < 0.1) fl->model_adjustment = 0.1;
    if (fl->model_adjustment > 2.0) fl->model_adjustment = 2.0;
}

int ns_feedback_should_explore(const NSFeedbackLoop *fl,
                               double exploration_threshold) {
    /* Explore if error is high (model is wrong) or RMSE exceeds threshold */
    return (fl->rmse > exploration_threshold) ? 1 : 0;
}

/* ── BatchAnalyzer ───────────────────────────────────────────────────── */

double ns_entropy(double p_avoid, double p_choose, double p_unknown) {
    double h = 0.0;
    if (p_avoid   > 0.0) h -= p_avoid   * log2(p_avoid);
    if (p_choose  > 0.0) h -= p_choose  * log2(p_choose);
    if (p_unknown > 0.0) h -= p_unknown * log2(p_unknown);
    return h;
}

double ns_kl_divergence(double p, double q) {
    if (p <= 0.0 || q <= 0.0) return 0.0;
    return p * log2(p / q);
}

NSBatchAnalysis ns_batch_analyze(const NSAvoidanceTracker *t, int n_batches) {
    NSBatchAnalysis a;
    memset(&a, 0, sizeof(a));

    if (t->count == 0 || n_batches <= 0 || n_batches > NS_MAX_BATCHES)
        return a;

    int per_batch = t->count / n_batches;
    if (per_batch == 0) per_batch = 1;

    double sum_entropy    = 0.0;
    double sum_confidence = 0.0;
    int    total_avoids   = 0;

    for (int b = 0; b < n_batches && b < NS_MAX_BATCHES; b++) {
        int start = b * per_batch;
        int end   = (b == n_batches - 1) ? t->count : (b + 1) * per_batch;
        if (start >= t->count) break;

        int avoids = 0, chooses = 0, unknowns = 0;
        double conf_sum = 0.0;

        for (int i = start; i < end && i < t->count; i++) {
            switch (t->actions[i].action) {
                case NS_ACTION_AVOID:  avoids++;  break;
                case NS_ACTION_CHOOSE: chooses++; break;
                case NS_ACTION_UNKNOWN: unknowns++; break;
            }
            conf_sum += t->actions[i].confidence;
        }

        int n = end - start;
        if (n <= 0) continue;

        double pa = (double)avoids  / n;
        double pc = (double)chooses / n;
        double pu = (double)unknowns / n;

        a.batches[b].batch_id        = b;
        a.batches[b].n_decisions     = n;
        a.batches[b].avoid_ratio     = pa;
        a.batches[b].choose_ratio    = pc;
        a.batches[b].entropy         = ns_entropy(pa, pc, pu);
        a.batches[b].mean_confidence = conf_sum / n;
        a.num_batches++;

        sum_entropy    += a.batches[b].entropy;
        sum_confidence += conf_sum;
        total_avoids   += avoids;
    }

    if (a.num_batches > 0) {
        a.overall_entropy = sum_entropy / a.num_batches;
        a.overall_mean_confidence = sum_confidence / t->count;
        int chooses = ns_tracker_count_by_type(t, NS_ACTION_CHOOSE);
        a.overall_avoid_choose_ratio = (chooses > 0) ?
            (double)total_avoids / (double)chooses : (double)total_avoids;
    }

    return a;
}
