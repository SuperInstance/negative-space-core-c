#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/negative_space_core.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ_DBL(name, a, b, eps) do {                                \
    if (fabs((a) - (b)) > (eps)) {                                          \
        printf("FAIL %s: %.6f != %.6f\n", name, (double)(a), (double)(b)); \
        tests_failed++;                                                      \
    } else { printf("PASS %s\n", name); tests_passed++; }                   \
} while(0)

#define ASSERT_EQ_INT(name, a, b) do {                                      \
    if ((a) != (b)) {                                                        \
        printf("FAIL %s: %d != %d\n", name, (int)(a), (int)(b));           \
        tests_failed++;                                                      \
    } else { printf("PASS %s\n", name); tests_passed++; }                   \
} while(0)

#define ASSERT_TRUE(name, cond) do {                                         \
    if (!(cond)) {                                                           \
        printf("FAIL %s\n", name); tests_failed++;                           \
    } else { printf("PASS %s\n", name); tests_passed++; }                   \
} while(0)

/* ── Test 1: Tracker init ──────────────────────────────────────────── */
static void test_tracker_init(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    ASSERT_EQ_INT("tracker_init_count", t.count, 0);
    ASSERT_EQ_DBL("tracker_init_avoid_ratio", ns_tracker_avoid_ratio(&t), 0.0, 1e-9);
}

/* ── Test 2: Record and count ──────────────────────────────────────── */
static void test_record_and_count(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    ns_tracker_record(&t, 1, NS_ACTION_AVOID, 0.9, 1.0);
    ns_tracker_record(&t, 2, NS_ACTION_CHOOSE, 0.7, 2.0);
    ns_tracker_record(&t, 3, NS_ACTION_UNKNOWN, 0.5, 3.0);
    ASSERT_EQ_INT("record_count", t.count, 3);
    ASSERT_EQ_INT("count_avoid", ns_tracker_count_by_type(&t, NS_ACTION_AVOID), 1);
    ASSERT_EQ_INT("count_choose", ns_tracker_count_by_type(&t, NS_ACTION_CHOOSE), 1);
    ASSERT_EQ_INT("count_unknown", ns_tracker_count_by_type(&t, NS_ACTION_UNKNOWN), 1);
}

/* ── Test 3: Ratios ────────────────────────────────────────────────── */
static void test_ratios(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    for (int i = 0; i < 10; i++)
        ns_tracker_record(&t, i, (i % 2 == 0) ? NS_ACTION_AVOID : NS_ACTION_CHOOSE,
                          0.8, (double)i);
    ASSERT_EQ_DBL("avoid_ratio_5_of_10", ns_tracker_avoid_ratio(&t), 0.5, 1e-9);
    ASSERT_EQ_DBL("choose_ratio_5_of_10", ns_tracker_choose_ratio(&t), 0.5, 1e-9);
    ASSERT_EQ_DBL("unknown_ratio_zero", ns_tracker_unknown_ratio(&t), 0.0, 1e-9);
}

/* ── Test 4: Standard deviation of confidence ──────────────────────── */
static void test_std(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    ns_tracker_record(&t, 1, NS_ACTION_AVOID, 1.0, 1.0);
    ns_tracker_record(&t, 2, NS_ACTION_AVOID, 0.0, 2.0);
    /* std of {1.0, 0.0} = 0.5 */
    ASSERT_EQ_DBL("std_two_values", ns_tracker_std(&t), 0.5, 1e-9);
}

/* ── Test 5: Tracker overflow ──────────────────────────────────────── */
static void test_tracker_overflow(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    int result = 0;
    for (int i = 0; i < NS_TRACKER_MAX_ACTIONS + 10; i++)
        result = ns_tracker_record(&t, i, NS_ACTION_AVOID, 0.5, (double)i);
    ASSERT_EQ_INT("tracker_count_capped", t.count, NS_TRACKER_MAX_ACTIONS);
    ASSERT_EQ_INT("last_overflow_returns_neg1", result, -1);
}

/* ── Test 6: Conservation — uniform avoidance ──────────────────────── */
static void test_conservation_uniform(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    /* 60% avoid across all actions → should be conserved */
    for (int i = 0; i < 100; i++) {
        NSActionType a = (i % 5 < 3) ? NS_ACTION_AVOID : NS_ACTION_CHOOSE;
        ns_tracker_record(&t, i, a, 0.8, (double)i);
    }
    NSConservationResult cr = ns_conservation_check(&t, 4, 0.05);
    ASSERT_EQ_INT("conservation_num_scales", cr.num_scales, 4);
    ASSERT_TRUE("conservation_is_conserved", cr.conserved);
    ASSERT_TRUE("conservation_low_variance", cr.variance < 0.05);
}

/* ── Test 7: Conservation — non-uniform ────────────────────────────── */
static void test_conservation_nonuniform(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    /* First half: all avoid. Second half: all choose */
    for (int i = 0; i < 50; i++)
        ns_tracker_record(&t, i, NS_ACTION_AVOID, 0.9, (double)i);
    for (int i = 50; i < 100; i++)
        ns_tracker_record(&t, i, NS_ACTION_CHOOSE, 0.9, (double)i);
    NSConservationResult cr = ns_conservation_check(&t, 2, 0.05);
    ASSERT_TRUE("nonuniform_not_conserved", !cr.conserved);
    ASSERT_TRUE("nonuniform_high_variance", cr.variance > 0.05);
}

/* ── Test 8: Inference — find gaps ─────────────────────────────────── */
static void test_inference_gaps(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    /* avoid block, gap of choose actions, avoid block */
    for (int i = 0; i < 5; i++)
        ns_tracker_record(&t, i, NS_ACTION_AVOID, 0.9, (double)i);
    for (int i = 5; i < 10; i++)
        ns_tracker_record(&t, i, NS_ACTION_CHOOSE, 0.5, (double)i);
    for (int i = 10; i < 15; i++)
        ns_tracker_record(&t, i, NS_ACTION_AVOID, 0.9, (double)i);
    NSInferenceResult ir = ns_inference_find_gaps(&t, 0.1);
    ASSERT_TRUE("inference_found_gaps", ir.num_gaps > 0);
    ASSERT_TRUE("inference_knowledge_positive", ir.total_inferred_knowledge > 0.0);
}

/* ── Test 9: Inference — no gaps ───────────────────────────────────── */
static void test_inference_no_gaps(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    /* All choose — no avoid regions → no gaps */
    for (int i = 0; i < 20; i++)
        ns_tracker_record(&t, i, NS_ACTION_CHOOSE, 0.8, (double)i);
    NSInferenceResult ir = ns_inference_find_gaps(&t, 0.1);
    ASSERT_EQ_INT("no_gaps_count", ir.num_gaps, 0);
}

/* ── Test 10: Feedback init and basic recording ────────────────────── */
static void test_feedback_basic(void) {
    NSFeedbackLoop fl;
    ns_feedback_init(&fl);
    ASSERT_EQ_INT("feedback_init_count", fl.count, 0);
    ASSERT_EQ_DBL("feedback_init_adjustment", fl.model_adjustment, 1.0, 1e-9);

    ns_feedback_record(&fl, 1, 0.8, 0.0);  /* predicted avoid, actual avoid */
    ns_feedback_record(&fl, 2, 0.3, 1.0);  /* predicted choose, actual choose */
    ASSERT_EQ_INT("feedback_record_count", fl.count, 2);
}

/* ── Test 11: Feedback update adjusts model ────────────────────────── */
static void test_feedback_update(void) {
    NSFeedbackLoop fl;
    ns_feedback_init(&fl);
    /* Overpredict avoidance: predict 0.9 avoid but always choose */
    for (int i = 0; i < 10; i++)
        ns_feedback_record(&fl, i, 0.9, 1.0);
    ns_feedback_update(&fl);
    ASSERT_TRUE("feedback_positive_mean_error", fl.mean_error > 0.0);
    ASSERT_TRUE("feedback_adjustment_decreased", fl.model_adjustment < 1.0);
    ASSERT_TRUE("feedback_rmse_positive", fl.rmse > 0.0);
}

/* ── Test 12: Feedback exploration trigger ─────────────────────────── */
static void test_feedback_exploration(void) {
    NSFeedbackLoop fl;
    ns_feedback_init(&fl);
    /* High errors → should explore */
    for (int i = 0; i < 10; i++)
        ns_feedback_record(&fl, i, 0.9, 0.0); /* error = -0.9 */
    ns_feedback_update(&fl);
    ASSERT_EQ_INT("should_explore_high_rmse", ns_feedback_should_explore(&fl, 0.1), 1);

    /* Low errors → should not explore */
    ns_feedback_init(&fl);
    for (int i = 0; i < 10; i++)
        ns_feedback_record(&fl, i, 0.5, 0.5); /* error = 0.0 */
    ns_feedback_update(&fl);
    ASSERT_EQ_INT("should_not_explore_low_rmse", ns_feedback_should_explore(&fl, 0.1), 0);
}

/* ── Test 13: Entropy calculation ──────────────────────────────────── */
static void test_entropy(void) {
    /* Uniform distribution over 3 outcomes → log2(3) ≈ 1.585 */
    double h = ns_entropy(1.0/3.0, 1.0/3.0, 1.0/3.0);
    ASSERT_TRUE("entropy_uniform_3", fabs(h - log2(3.0)) < 0.001);

    /* Deterministic → entropy 0 */
    double h0 = ns_entropy(1.0, 0.0, 0.0);
    ASSERT_EQ_DBL("entropy_deterministic", h0, 0.0, 1e-9);
}

/* ── Test 14: KL divergence ────────────────────────────────────────── */
static void test_kl_divergence(void) {
    double kl = ns_kl_divergence(0.5, 0.5);
    ASSERT_EQ_DBL("kl_same_distribution", kl, 0.0, 1e-9);
    double kl2 = ns_kl_divergence(0.8, 0.2);
    ASSERT_TRUE("kl_different_positive", kl2 > 0.0);
}

/* ── Test 15: Batch analysis ───────────────────────────────────────── */
static void test_batch_analysis(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    /* 100 actions: 60 avoid, 30 choose, 10 unknown */
    for (int i = 0; i < 60; i++)
        ns_tracker_record(&t, i, NS_ACTION_AVOID, 0.8, (double)i);
    for (int i = 60; i < 90; i++)
        ns_tracker_record(&t, i, NS_ACTION_CHOOSE, 0.7, (double)i);
    for (int i = 90; i < 100; i++)
        ns_tracker_record(&t, i, NS_ACTION_UNKNOWN, 0.3, (double)i);

    NSBatchAnalysis ba = ns_batch_analyze(&t, 5);
    ASSERT_EQ_INT("batch_num_batches", ba.num_batches, 5);
    ASSERT_TRUE("batch_overall_entropy_positive", ba.overall_entropy > 0.0);
    ASSERT_TRUE("batch_avoid_choose_ratio_2x", fabs(ba.overall_avoid_choose_ratio - 2.0) < 0.1);
    ASSERT_TRUE("batch_mean_confidence", ba.overall_mean_confidence > 0.5);
}

/* ── Test 16: Conservation edge case — empty tracker ───────────────── */
static void test_conservation_empty(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    NSConservationResult cr = ns_conservation_check(&t, 3, 0.05);
    ASSERT_EQ_INT("conservation_empty_scales", cr.num_scales, 0);
}

/* ── Test 17: Batch edge case — more batches than actions ──────────── */
static void test_batch_oversplit(void) {
    NSAvoidanceTracker t;
    ns_tracker_init(&t);
    ns_tracker_record(&t, 1, NS_ACTION_AVOID, 0.9, 1.0);
    ns_tracker_record(&t, 2, NS_ACTION_CHOOSE, 0.8, 2.0);
    NSBatchAnalysis ba = ns_batch_analyze(&t, 100);
    ASSERT_TRUE("batch_oversplit_fewer", ba.num_batches <= 2);
}

/* ── Test 18: Feedback model adjustment bounds ─────────────────────── */
static void test_feedback_bounds(void) {
    NSFeedbackLoop fl;
    ns_feedback_init(&fl);
    /* Extreme errors → model adjustment should clamp */
    for (int i = 0; i < 100; i++)
        ns_feedback_record(&fl, i, 0.01, 1.0); /* huge positive error */
    ns_feedback_update(&fl);
    ASSERT_TRUE("adjustment_lower_bound", fl.model_adjustment >= 0.1);
}

/* ── Main ──────────────────────────────────────────────────────────── */
int main(void) {
    printf("=== Negative Space Core — Test Suite ===\n\n");

    test_tracker_init();
    test_record_and_count();
    test_ratios();
    test_std();
    test_tracker_overflow();
    test_conservation_uniform();
    test_conservation_nonuniform();
    test_inference_gaps();
    test_inference_no_gaps();
    test_feedback_basic();
    test_feedback_update();
    test_feedback_exploration();
    test_entropy();
    test_kl_divergence();
    test_batch_analysis();
    test_conservation_empty();
    test_batch_oversplit();
    test_feedback_bounds();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
