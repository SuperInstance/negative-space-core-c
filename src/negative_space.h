#ifndef NEGATIVE_SPACE_H
#define NEGATIVE_SPACE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Negative Space Intelligence Core
 *
 * Intelligence is what you learn to AVOID.
 * This library tracks avoidance patterns, identifies negative
 * regions, verifies conservation laws across scales, performs
 * gap-based inference, and maintains feedback loops.
 * ============================================================ */

/* --- AvoidanceTracker --- */

typedef struct {
    int position;            /* spatial or abstract position */
    int total_observations;  /* how many times this position was evaluated */
    int avoidances;          /* how many times it was avoided */
} AvoidanceEntry;

typedef struct {
    AvoidanceEntry *entries;
    int count;
    int capacity;
} AvoidanceTracker;

/* Initialize/destroy */
void avoidance_tracker_init(AvoidanceTracker *t, int capacity_hint);
void avoidance_tracker_free(AvoidanceTracker *t);

/* Record that position was evaluated; avoided=true means agent avoided it */
void avoidance_record(AvoidanceTracker *t, int position, bool avoided);

/* Get avoidance ratio for a position (0.0 if no observations) */
double avoidance_ratio(const AvoidanceTracker *t, int position);

/* Get global avoidance ratio across all positions */
double avoidance_ratio_global(const AvoidanceTracker *t);

/* Get entry for position, or NULL */
const AvoidanceEntry *avoidance_get_entry(const AvoidanceTracker *t, int position);

/* --- NegativeRegion --- */

typedef struct {
    int start;     /* inclusive start position */
    int end;       /* inclusive end position */
    double avg_avoidance_ratio;
} NegativeRegion;

typedef struct {
    NegativeRegion *regions;
    int count;
    int capacity;
} NegativeRegionResult;

/* Identify contiguous regions where avoidance ratio >= threshold.
 * Positions must be sorted; caller provides min_region_size. */
NegativeRegionResult negative_regions_find(const AvoidanceTracker *t,
                                            double threshold,
                                            int min_region_size);

void negative_regions_free(NegativeRegionResult *r);

/* Get boundary of a region (start-1 and end+1 avoidance ratios, if they exist) */
bool negative_region_boundary(const AvoidanceTracker *t,
                               const NegativeRegion *region,
                               double *before_ratio,
                               double *after_ratio);

/* --- ConservationLaw --- */

typedef struct {
    double mean;
    double std_dev;
    bool conserved;
} ConservationResult;

/* Check that avoidance ratio std dev across scales is below threshold.
 * "Scales" = partitions of positions into groups of `group_size`. */
ConservationResult conservation_check(const AvoidanceTracker *t,
                                       int group_size,
                                       double max_std);

/* --- InferenceEngine --- */

/* What does the gap between two avoided positions imply?
 * If positions A and B are both avoided, the gap (A..B) may contain
 * positions that are implied-avoided (interpolation) or implied-present
 * (exclusion — agents go around, not through). */

typedef enum {
    INFERENCE_INTERPOLATE,   /* gap positions are likely also avoided */
    INFERENCE_EXCLUSION,     /* gap positions are likely present (agents go around) */
    INFERENCE_UNKNOWN
} InferenceType;

typedef struct {
    int position;
    double confidence;       /* 0.0 to 1.0 */
    InferenceType type;
} InferredPosition;

typedef struct {
    InferredPosition *positions;
    int count;
    int capacity;
} InferenceResult;

/* Infer what lies in the gap between two known-avoided positions */
InferenceResult infer_gap(const AvoidanceTracker *t,
                           int pos_a, int pos_b);

void inference_result_free(InferenceResult *r);

/* --- FeedbackLoop --- */

typedef struct {
    AvoidanceTracker *tracker;
    double learning_rate;    /* how much to adjust on new evidence */
    int total_feedback;      /* total feedback events processed */
    int correct_predictions; /* times avoidance prediction matched outcome */
} FeedbackLoop;

void feedback_loop_init(FeedbackLoop *fl, AvoidanceTracker *t, double learning_rate);
void feedback_loop_free(FeedbackLoop *fl);

/* Update model: position was predicted_avoided, actual was actual_avoided */
void feedback_update(FeedbackLoop *fl, int position,
                     bool predicted_avoided, bool actual_avoided);

/* Get prediction accuracy (0.0 to 1.0) */
double feedback_accuracy(const FeedbackLoop *fl);

/* Predict whether a position should be avoided (threshold-based) */
bool feedback_predict(const FeedbackLoop *fl, int position, double threshold);

#ifdef __cplusplus
}
#endif

#endif /* NEGATIVE_SPACE_H */
