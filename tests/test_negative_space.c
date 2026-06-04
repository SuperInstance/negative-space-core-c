#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../src/negative_space.h"

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  %-50s", #name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

#define ASSERT_EQ_DBL(a, b, eps) do { \
    if (fabs((a) - (b)) > (eps)) { \
        printf("FAIL: %.6f != %.6f (line %d)\n", (double)(a), (double)(b), __LINE__); \
        return; \
    } \
} while(0)

#define ASSERT_EQ_INT(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL: %d != %d (line %d)\n", (int)(a), (int)(b), __LINE__); \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        printf("FAIL: assertion false (line %d)\n", __LINE__); \
        return; \
    } \
} while(0)

/* --- Tests --- */

void test_avoidance_record_and_ratio(void) {
    TEST(test_avoidance_record_and_ratio);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 8);

    avoidance_record(&t, 1, true);
    avoidance_record(&t, 1, true);
    avoidance_record(&t, 1, false);

    ASSERT_EQ_DBL(avoidance_ratio(&t, 1), 2.0/3.0, 1e-9);
    ASSERT_EQ_DBL(avoidance_ratio(&t, 2), 0.0, 1e-9);

    avoidance_tracker_free(&t);
    PASS();
}

void test_global_avoidance_ratio(void) {
    TEST(test_global_avoidance_ratio);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 8);

    avoidance_record(&t, 0, true);
    avoidance_record(&t, 0, false);
    avoidance_record(&t, 1, true);
    avoidance_record(&t, 1, true);

    ASSERT_EQ_DBL(avoidance_ratio_global(&t), 3.0/4.0, 1e-9);

    avoidance_tracker_free(&t);
    PASS();
}

void test_get_entry(void) {
    TEST(test_get_entry);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 8);

    avoidance_record(&t, 5, true);
    avoidance_record(&t, 10, false);

    const AvoidanceEntry *e = avoidance_get_entry(&t, 5);
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ_INT(e->avoidances, 1);
    ASSERT_EQ_INT(e->total_observations, 1);

    ASSERT_TRUE(avoidance_get_entry(&t, 7) == NULL);

    avoidance_tracker_free(&t);
    PASS();
}

void test_tracker_expansion(void) {
    TEST(test_tracker_expansion);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 2);

    for (int i = 0; i < 20; i++) {
        avoidance_record(&t, i, i % 2 == 0);
    }
    ASSERT_EQ_INT(t.count, 20);

    avoidance_tracker_free(&t);
    PASS();
}

void test_find_negative_regions(void) {
    TEST(test_find_negative_regions);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);

    /* Positions 0-4 all avoided 90%+, positions 5-6 not */
    for (int p = 0; p <= 4; p++) {
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);
        avoidance_record(&t, p, false);
    }
    for (int p = 5; p <= 6; p++) {
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, false);
        avoidance_record(&t, p, true);
    }

    NegativeRegionResult r = negative_regions_find(&t, 0.8, 3);
    ASSERT_EQ_INT(r.count, 1);
    ASSERT_EQ_INT(r.regions[0].start, 0);
    ASSERT_EQ_INT(r.regions[0].end, 4);
    ASSERT_TRUE(r.regions[0].avg_avoidance_ratio > 0.85);

    negative_regions_free(&r);
    avoidance_tracker_free(&t);
    PASS();
}

void test_negative_regions_empty(void) {
    TEST(test_negative_regions_empty);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 8);

    NegativeRegionResult r = negative_regions_find(&t, 0.5, 1);
    ASSERT_EQ_INT(r.count, 0);

    negative_regions_free(&r);
    avoidance_tracker_free(&t);
    PASS();
}

void test_negative_region_min_size(void) {
    TEST(test_negative_region_min_size);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);

    /* Two separate small clusters: size 2 and size 5 */
    for (int p = 0; p <= 1; p++)
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);

    for (int p = 10; p <= 14; p++)
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);

    NegativeRegionResult r = negative_regions_find(&t, 0.8, 3);
    ASSERT_EQ_INT(r.count, 1);
    ASSERT_EQ_INT(r.regions[0].start, 10);
    ASSERT_EQ_INT(r.regions[0].end, 14);

    negative_regions_free(&r);
    avoidance_tracker_free(&t);
    PASS();
}

void test_region_boundary(void) {
    TEST(test_region_boundary);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);

    /* Region at 2-4, surrounded by non-avoided at 1 and 5 */
    for (int i = 0; i < 10; i++) avoidance_record(&t, 1, false);
    for (int p = 2; p <= 4; p++)
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);
    for (int i = 0; i < 10; i++) avoidance_record(&t, 5, false);
    avoidance_record(&t, 5, false);

    NegativeRegionResult r = negative_regions_find(&t, 0.8, 3);
    ASSERT_TRUE(r.count >= 1);

    double before, after;
    bool found = negative_region_boundary(&t, &r.regions[0], &before, &after);
    ASSERT_TRUE(found);
    ASSERT_TRUE(before < 0.2);
    ASSERT_TRUE(after < 0.2);

    negative_regions_free(&r);
    avoidance_tracker_free(&t);
    PASS();
}

void test_conservation_uniform(void) {
    TEST(test_conservation_uniform);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 32);

    /* Uniform avoidance ~50% across all positions → should be conserved */
    for (int p = 0; p < 20; p++) {
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, false);
    }

    ConservationResult c = conservation_check(&t, 5, 0.15);
    ASSERT_TRUE(c.conserved);
    ASSERT_TRUE(c.std_dev < 0.15);

    avoidance_tracker_free(&t);
    PASS();
}

void test_conservation_non_uniform(void) {
    TEST(test_conservation_non_uniform);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 32);

    /* First half 90% avoided, second half 10% → not conserved */
    for (int p = 0; p < 10; p++)
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);
    for (int p = 10; p < 20; p++)
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, false);

    ConservationResult c = conservation_check(&t, 10, 0.3);
    ASSERT_TRUE(!c.conserved);
    ASSERT_TRUE(c.std_dev > 0.3);

    avoidance_tracker_free(&t);
    PASS();
}

void test_infer_gap_interpolation(void) {
    TEST(test_infer_gap_interpolation);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);

    /* Two strongly avoided positions with a gap */
    for (int i = 0; i < 10; i++) avoidance_record(&t, 0, true);
    for (int i = 0; i < 10; i++) avoidance_record(&t, 5, true);

    InferenceResult ir = infer_gap(&t, 0, 5);
    ASSERT_TRUE(ir.count > 0);

    /* Gap positions should be interpolated as avoided */
    for (int i = 0; i < ir.count; i++) {
        ASSERT_TRUE(ir.positions[i].type == INFERENCE_INTERPOLATE);
        ASSERT_TRUE(ir.positions[i].confidence > 0.6);
    }

    inference_result_free(&ir);
    avoidance_tracker_free(&t);
    PASS();
}

void test_infer_gap_exclusion(void) {
    TEST(test_infer_gap_exclusion);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);

    /* Two barely-avoided positions (agents go around → exclusion) */
    for (int i = 0; i < 10; i++) avoidance_record(&t, 0, false);
    for (int i = 0; i < 10; i++) avoidance_record(&t, 5, false);

    InferenceResult ir = infer_gap(&t, 0, 5);
    ASSERT_TRUE(ir.count > 0);

    for (int i = 0; i < ir.count; i++) {
        ASSERT_TRUE(ir.positions[i].type == INFERENCE_EXCLUSION);
    }

    inference_result_free(&ir);
    avoidance_tracker_free(&t);
    PASS();
}

void test_infer_gap_no_gap(void) {
    TEST(test_infer_gap_no_gap);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 8);

    /* Adjacent positions → no gap */
    for (int i = 0; i < 10; i++) avoidance_record(&t, 0, true);
    for (int i = 0; i < 10; i++) avoidance_record(&t, 1, true);

    InferenceResult ir = infer_gap(&t, 0, 1);
    ASSERT_EQ_INT(ir.count, 0);

    inference_result_free(&ir);
    avoidance_tracker_free(&t);
    PASS();
}

void test_feedback_loop_accuracy(void) {
    TEST(test_feedback_loop_accuracy);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);
    FeedbackLoop fl;
    feedback_loop_init(&fl, &t, 0.5);

    /* Record some baseline */
    for (int i = 0; i < 10; i++) avoidance_record(&t, 0, true);

    /* Predict avoided, actual avoided → correct */
    feedback_update(&fl, 0, true, true);
    ASSERT_EQ_DBL(feedback_accuracy(&fl), 1.0, 1e-9);

    /* Predict avoided, actual not → incorrect */
    feedback_update(&fl, 1, true, false);
    ASSERT_EQ_DBL(feedback_accuracy(&fl), 0.5, 1e-9);

    feedback_loop_free(&fl);
    avoidance_tracker_free(&t);
    PASS();
}

void test_feedback_predict(void) {
    TEST(test_feedback_predict);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);
    FeedbackLoop fl;
    feedback_loop_init(&fl, &t, 0.5);

    /* Position 0: 80% avoided */
    for (int i = 0; i < 8; i++) avoidance_record(&t, 0, true);
    for (int i = 0; i < 2; i++) avoidance_record(&t, 0, false);

    /* Position 1: 20% avoided */
    for (int i = 0; i < 2; i++) avoidance_record(&t, 1, true);
    for (int i = 0; i < 8; i++) avoidance_record(&t, 1, false);

    ASSERT_TRUE(feedback_predict(&fl, 0, 0.5));
    ASSERT_TRUE(!feedback_predict(&fl, 1, 0.5));

    feedback_loop_free(&fl);
    avoidance_tracker_free(&t);
    PASS();
}

void test_feedback_adjusts_model(void) {
    TEST(test_feedback_adjusts_model);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 16);
    FeedbackLoop fl;
    feedback_loop_init(&fl, &t, 0.5);

    /* Start neutral */
    avoidance_record(&t, 0, true);
    avoidance_record(&t, 0, false);
    ASSERT_EQ_DBL(avoidance_ratio(&t, 0), 0.5, 1e-9);

    /* Feedback: predicted not-avoided, but was actually avoided → reinforce */
    feedback_update(&fl, 0, false, true);
    /* Should have increased avoidance ratio due to reinforcement */
    ASSERT_TRUE(avoidance_ratio(&t, 0) > 0.5);

    feedback_loop_free(&fl);
    avoidance_tracker_free(&t);
    PASS();
}

void test_full_pipeline(void) {
    TEST(test_full_pipeline);
    AvoidanceTracker t;
    avoidance_tracker_init(&t, 64);
    FeedbackLoop fl;
    feedback_loop_init(&fl, &t, 0.5);

    /* Simulate: agents avoid positions 10-20, navigate through 0-9 and 21-30 */
    for (int p = 10; p <= 20; p++) {
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, true);
        avoidance_record(&t, p, false);
    }
    for (int p = 0; p <= 9; p++) {
        avoidance_record(&t, p, false);
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, false);
    }
    for (int p = 21; p <= 30; p++) {
        avoidance_record(&t, p, false);
        for (int i = 0; i < 10; i++) avoidance_record(&t, p, false);
    }

    /* Find negative regions */
    NegativeRegionResult r = negative_regions_find(&t, 0.8, 3);
    ASSERT_TRUE(r.count >= 1);

    /* Check conservation */
    ConservationResult c = conservation_check(&t, 10, 0.3);
    /* Non-uniform so should not be conserved */
    ASSERT_TRUE(!c.conserved);

    /* Infer gap between two avoided positions with unknown gap */
    /* Position 10 and 20 are avoided, 11-19 has a mix of known/unknown */
    /* Clear gap: use positions 30 and 35 which have no data */
    InferenceResult ir = infer_gap(&t, 30, 35);
    ASSERT_TRUE(ir.count > 0);

    /* Feedback loop */
    feedback_update(&fl, 10, true, true);
    feedback_update(&fl, 5, false, false);
    ASSERT_EQ_DBL(feedback_accuracy(&fl), 1.0, 1e-9);

    inference_result_free(&ir);
    negative_regions_free(&r);
    feedback_loop_free(&fl);
    avoidance_tracker_free(&t);
    PASS();
}

/* --- Main --- */

int main(void) {
    printf("\n=== Negative Space Core C — Test Suite ===\n\n");

    test_avoidance_record_and_ratio();
    test_global_avoidance_ratio();
    test_get_entry();
    test_tracker_expansion();
    test_find_negative_regions();
    test_negative_regions_empty();
    test_negative_region_min_size();
    test_region_boundary();
    test_conservation_uniform();
    test_conservation_non_uniform();
    test_infer_gap_interpolation();
    test_infer_gap_exclusion();
    test_infer_gap_no_gap();
    test_feedback_loop_accuracy();
    test_feedback_predict();
    test_feedback_adjusts_model();
    test_full_pipeline();

    printf("\n%d/%d tests passed\n", tests_passed, tests_total);

    if (tests_passed != tests_total) {
        printf("SOME TESTS FAILED!\n");
        return 1;
    }

    printf("All tests passed!\n\n");
    return 0;
}
