#include "negative_space.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * AvoidanceTracker
 * ============================================================ */

static AvoidanceEntry *find_entry(AvoidanceTracker *t, int position) {
    for (int i = 0; i < t->count; i++) {
        if (t->entries[i].position == position)
            return &t->entries[i];
    }
    return NULL;
}

void avoidance_tracker_init(AvoidanceTracker *t, int capacity_hint) {
    t->capacity = capacity_hint > 0 ? capacity_hint : 16;
    t->entries = calloc(t->capacity, sizeof(AvoidanceEntry));
    t->count = 0;
}

void avoidance_tracker_free(AvoidanceTracker *t) {
    free(t->entries);
    t->entries = NULL;
    t->count = t->capacity = 0;
}

static void ensure_capacity(AvoidanceTracker *t) {
    if (t->count >= t->capacity) {
        t->capacity *= 2;
        t->entries = realloc(t->entries, t->capacity * sizeof(AvoidanceEntry));
        memset(t->entries + t->count, 0,
               (t->capacity - t->count) * sizeof(AvoidanceEntry));
    }
}

void avoidance_record(AvoidanceTracker *t, int position, bool avoided) {
    AvoidanceEntry *e = find_entry(t, position);
    if (!e) {
        ensure_capacity(t);
        e = &t->entries[t->count++];
        e->position = position;
        e->total_observations = 0;
        e->avoidances = 0;
    }
    e->total_observations++;
    if (avoided) e->avoidances++;
}

double avoidance_ratio(const AvoidanceTracker *t, int position) {
    const AvoidanceEntry *e = avoidance_get_entry(t, position);
    if (!e || e->total_observations == 0) return 0.0;
    return (double)e->avoidances / e->total_observations;
}

double avoidance_ratio_global(const AvoidanceTracker *t) {
    int total_obs = 0, total_avoid = 0;
    for (int i = 0; i < t->count; i++) {
        total_obs += t->entries[i].total_observations;
        total_avoid += t->entries[i].avoidances;
    }
    if (total_obs == 0) return 0.0;
    return (double)total_avoid / total_obs;
}

const AvoidanceEntry *avoidance_get_entry(const AvoidanceTracker *t, int position) {
    for (int i = 0; i < t->count; i++) {
        if (t->entries[i].position == position)
            return &t->entries[i];
    }
    return NULL;
}

/* ============================================================
 * NegativeRegion
 * ============================================================ */

/* Comparator for sorting entries by position */
static int cmp_entry(const void *a, const void *b) {
    return ((const AvoidanceEntry *)a)->position -
           ((const AvoidanceEntry *)b)->position;
}

NegativeRegionResult negative_regions_find(const AvoidanceTracker *t,
                                            double threshold,
                                            int min_region_size) {
    NegativeRegionResult result = {NULL, 0, 0};

    if (t->count == 0) return result;

    /* Sort by position */
    AvoidanceEntry *sorted = malloc(t->count * sizeof(AvoidanceEntry));
    memcpy(sorted, t->entries, t->count * sizeof(AvoidanceEntry));
    qsort(sorted, t->count, sizeof(AvoidanceEntry), cmp_entry);

    result.capacity = 8;
    result.regions = malloc(result.capacity * sizeof(NegativeRegion));
    result.count = 0;

    int i = 0;
    while (i < t->count) {
        double ratio = (sorted[i].total_observations > 0)
            ? (double)sorted[i].avoidances / sorted[i].total_observations
            : 0.0;

        if (ratio >= threshold) {
            int start = sorted[i].position;
            int end = start;
            int run_count = 1;
            double sum_ratio = ratio;

            /* Extend contiguous run */
            int j = i + 1;
            while (j < t->count && sorted[j].position == end + 1) {
                double r = (sorted[j].total_observations > 0)
                    ? (double)sorted[j].avoidances / sorted[j].total_observations
                    : 0.0;
                if (r < threshold) break;
                end = sorted[j].position;
                sum_ratio += r;
                run_count++;
                j++;
            }

            if (run_count >= min_region_size) {
                if (result.count >= result.capacity) {
                    result.capacity *= 2;
                    result.regions = realloc(result.regions,
                        result.capacity * sizeof(NegativeRegion));
                }
                result.regions[result.count].start = start;
                result.regions[result.count].end = end;
                result.regions[result.count].avg_avoidance_ratio = sum_ratio / run_count;
                result.count++;
            }
            i = j;
        } else {
            i++;
        }
    }

    free(sorted);
    return result;
}

void negative_regions_free(NegativeRegionResult *r) {
    free(r->regions);
    r->regions = NULL;
    r->count = r->capacity = 0;
}

bool negative_region_boundary(const AvoidanceTracker *t,
                               const NegativeRegion *region,
                               double *before_ratio,
                               double *after_ratio) {
    bool found_before = false, found_after = false;
    *before_ratio = 0.0;
    *after_ratio = 0.0;

    const AvoidanceEntry *e = avoidance_get_entry(t, region->start - 1);
    if (e) { *before_ratio = (double)e->avoidances / e->total_observations; found_before = true; }

    e = avoidance_get_entry(t, region->end + 1);
    if (e) { *after_ratio = (double)e->avoidances / e->total_observations; found_after = true; }

    return found_before || found_after;
}

/* ============================================================
 * ConservationLaw
 * ============================================================ */

ConservationResult conservation_check(const AvoidanceTracker *t,
                                       int group_size,
                                       double max_std) {
    ConservationResult result = {0.0, 0.0, false};

    if (t->count == 0 || group_size <= 0) return result;

    /* Sort by position */
    AvoidanceEntry *sorted = malloc(t->count * sizeof(AvoidanceEntry));
    memcpy(sorted, t->entries, t->count * sizeof(AvoidanceEntry));
    qsort(sorted, t->count, sizeof(AvoidanceEntry), cmp_entry);

    /* Compute per-group avoidance ratios */
    int n_groups = (t->count + group_size - 1) / group_size;
    double *group_ratios = malloc(n_groups * sizeof(double));

    for (int g = 0; g < n_groups; g++) {
        int total_obs = 0, total_avoid = 0;
        int start_idx = g * group_size;
        int end_idx = start_idx + group_size;
        if (end_idx > t->count) end_idx = t->count;

        for (int i = start_idx; i < end_idx; i++) {
            total_obs += sorted[i].total_observations;
            total_avoid += sorted[i].avoidances;
        }
        group_ratios[g] = (total_obs > 0) ? (double)total_avoid / total_obs : 0.0;
    }

    /* Mean */
    double sum = 0.0;
    for (int i = 0; i < n_groups; i++) sum += group_ratios[i];
    result.mean = sum / n_groups;

    /* Std dev */
    double sum_sq = 0.0;
    for (int i = 0; i < n_groups; i++) {
        double diff = group_ratios[i] - result.mean;
        sum_sq += diff * diff;
    }
    result.std_dev = sqrt(sum_sq / n_groups);
    result.conserved = (result.std_dev < max_std);

    free(sorted);
    free(group_ratios);
    return result;
}

/* ============================================================
 * InferenceEngine
 * ============================================================ */

static void inference_ensure_cap(InferenceResult *r) {
    if (r->count >= r->capacity) {
        r->capacity = r->capacity ? r->capacity * 2 : 8;
        r->positions = realloc(r->positions, r->capacity * sizeof(InferredPosition));
    }
}

InferenceResult infer_gap(const AvoidanceTracker *t, int pos_a, int pos_b) {
    InferenceResult result = {NULL, 0, 0};

    if (pos_a >= pos_b - 1) return result;

    double ratio_a = avoidance_ratio(t, pos_a);
    double ratio_b = avoidance_ratio(t, pos_b);
    double avg_avoidance = (ratio_a + ratio_b) / 2.0;

    result.capacity = 8;
    result.positions = malloc(result.capacity * sizeof(InferredPosition));
    result.count = 0;

    for (int p = pos_a + 1; p < pos_b; p++) {
        const AvoidanceEntry *e = avoidance_get_entry(t, p);

        if (e && e->total_observations > 0) {
            /* Known position — skip or low confidence */
            continue;
        }

        /* Unknown position: interpolate */
        double t_frac = (double)(p - pos_a) / (pos_b - pos_a);
        double interpolated = ratio_a + (ratio_b - ratio_a) * t_frac;

        InferenceType type;
        double confidence;

        if (avg_avoidance > 0.7) {
            /* Both strongly avoided → gap likely avoided too */
            type = INFERENCE_INTERPOLATE;
            confidence = interpolated;
        } else if (avg_avoidance < 0.3) {
            /* Both barely avoided → gap is exclusion zone */
            type = INFERENCE_EXCLUSION;
            confidence = 1.0 - interpolated;
        } else {
            type = INFERENCE_UNKNOWN;
            confidence = 0.5;
        }

        inference_ensure_cap(&result);
        result.positions[result.count].position = p;
        result.positions[result.count].confidence = confidence;
        result.positions[result.count].type = type;
        result.count++;
    }

    return result;
}

void inference_result_free(InferenceResult *r) {
    free(r->positions);
    r->positions = NULL;
    r->count = r->capacity = 0;
}

/* ============================================================
 * FeedbackLoop
 * ============================================================ */

void feedback_loop_init(FeedbackLoop *fl, AvoidanceTracker *t, double learning_rate) {
    fl->tracker = t;
    fl->learning_rate = learning_rate;
    fl->total_feedback = 0;
    fl->correct_predictions = 0;
}

void feedback_loop_free(FeedbackLoop *fl) {
    /* Nothing extra to free — tracker is owned externally */
    fl->tracker = NULL;
}

void feedback_update(FeedbackLoop *fl, int position,
                     bool predicted_avoided, bool actual_avoided) {
    fl->total_feedback++;

    if (predicted_avoided == actual_avoided) {
        fl->correct_predictions++;
    }

    /* Adjust observations: if prediction was wrong, add extra weight */
    if (predicted_avoided && !actual_avoided) {
        /* Over-avoided — add positive experience */
        avoidance_record(fl->tracker, position, false);
        avoidance_record(fl->tracker, position, false);
    } else if (!predicted_avoided && actual_avoided) {
        /* Under-avoided — reinforce avoidance */
        avoidance_record(fl->tracker, position, true);
        avoidance_record(fl->tracker, position, true);
    }

    /* Always record the actual outcome */
    avoidance_record(fl->tracker, position, actual_avoided);
}

double feedback_accuracy(const FeedbackLoop *fl) {
    if (fl->total_feedback == 0) return 0.0;
    return (double)fl->correct_predictions / fl->total_feedback;
}

bool feedback_predict(const FeedbackLoop *fl, int position, double threshold) {
    double ratio = avoidance_ratio(fl->tracker, position);
    return ratio >= threshold;
}
