#ifndef NEGATIVE_SPACE_CORE_H
#define NEGATIVE_SPACE_CORE_H

#include <stddef.h>
#include <math.h>

/*
 * Negative Space Intelligence Theory — Core C Library
 *
 * Intelligence is revealed not by what an entity chooses, but by what it avoids.
 * This library implements the core primitives: tracking avoidance, verifying
 * conservation laws, inferring knowledge from gaps, and closing feedback loops.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Common Types ────────────────────────────────────────────────────── */

typedef enum {
    NS_ACTION_AVOID   = 0,
    NS_ACTION_CHOOSE  = 1,
    NS_ACTION_UNKNOWN = 2
} NSActionType;

typedef struct {
    int      id;
    NSActionType action;
    double   confidence;   /* 0.0 – 1.0 */
    double   timestamp;    /* abstract time units */
} NSAction;

/* ── AvoidanceTracker ────────────────────────────────────────────────── */

#define NS_TRACKER_MAX_ACTIONS 4096

typedef struct {
    NSAction actions[NS_TRACKER_MAX_ACTIONS];
    int      count;
} NSAvoidanceTracker;

void    ns_tracker_init(NSAvoidanceTracker *t);
int     ns_tracker_record(NSAvoidanceTracker *t, int id, NSActionType action,
                          double confidence, double timestamp);
double  ns_tracker_avoid_ratio(const NSAvoidanceTracker *t);
double  ns_tracker_choose_ratio(const NSAvoidanceTracker *t);
double  ns_tracker_unknown_ratio(const NSAvoidanceTracker *t);
double  ns_tracker_std(const NSAvoidanceTracker *t);
int     ns_tracker_count_by_type(const NSAvoidanceTracker *t, NSActionType type);

/* ── ConservationLaw ─────────────────────────────────────────────────── */

#define NS_MAX_SCALES 16

typedef struct {
    double avoid_ratio;
    int    n_actions;
    double timestamp_start;
    double timestamp_end;
} NSScaleResult;

typedef struct {
    NSScaleResult scales[NS_MAX_SCALES];
    int           num_scales;
    double        variance;       /* variance of avoid_ratios across scales */
    int           conserved;      /* 1 if variance < threshold */
} NSConservationResult;

NSConservationResult ns_conservation_check(const NSAvoidanceTracker *t,
                                           int num_scales, double threshold);

/* ── InferenceEngine ─────────────────────────────────────────────────── */

#define NS_MAX_GAPS 256

typedef struct {
    int    gap_start_id;
    int    gap_end_id;
    double knowledge_estimate;   /* inferred knowledge strength 0–1 */
    double confidence;
} NSGap;

typedef struct {
    NSGap  gaps[NS_MAX_GAPS];
    int    num_gaps;
    double total_inferred_knowledge; /* 0–1 aggregate */
} NSInferenceResult;

NSInferenceResult ns_inference_find_gaps(const NSAvoidanceTracker *t,
                                         double min_gap_width);

/* ── FeedbackLoop ────────────────────────────────────────────────────── */

typedef struct {
    int    action_id;
    double predicted_avoid_prob;  /* what we predicted */
    double actual_outcome;        /* 0 = avoided, 1 = chosen */
    double error;                 /* computed: actual - predicted */
} NSFeedbackEntry;

typedef struct {
    NSFeedbackEntry entries[NS_TRACKER_MAX_ACTIONS];
    int             count;
    double          mean_error;
    double          rmse;
    double          model_adjustment;  /* accumulated adjustment factor */
} NSFeedbackLoop;

void ns_feedback_init(NSFeedbackLoop *fl);
int  ns_feedback_record(NSFeedbackLoop *fl, int action_id,
                        double predicted, double actual);
void ns_feedback_update(NSFeedbackLoop *fl);
int  ns_feedback_should_explore(const NSFeedbackLoop *fl,
                                double exploration_threshold);

/* ── BatchAnalyzer ───────────────────────────────────────────────────── */

#define NS_MAX_BATCHES 128

typedef struct {
    int    batch_id;
    int    n_decisions;
    double avoid_ratio;
    double choose_ratio;
    double entropy;             /* Shannon entropy of action distribution */
    double mean_confidence;
} NSBatchResult;

typedef struct {
    NSBatchResult batches[NS_MAX_BATCHES];
    int           num_batches;
    double        overall_entropy;
    double        overall_avoid_choose_ratio;
    double        overall_mean_confidence;
} NSBatchAnalysis;

/* Batch boundaries: split tracker into n_batches equal slices */
NSBatchAnalysis ns_batch_analyze(const NSAvoidanceTracker *t, int n_batches);

/* Standalone utility functions */
double ns_entropy(double p_avoid, double p_choose, double p_unknown);
double ns_kl_divergence(double p, double q);

#ifdef __cplusplus
}
#endif

#endif /* NEGATIVE_SPACE_CORE_H */
