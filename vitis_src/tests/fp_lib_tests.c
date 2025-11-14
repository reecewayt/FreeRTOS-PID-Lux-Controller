#include <stdio.h>
#include <stdlib.h>
#include <math.h> // For fabs() in verification

// --- Configuration ---

// 1. DEFINE a Q-format.
// We are choosing Q16.15 (17 whole bits, 15 fractional)
// This is the library's default, but we set it explicitly to be clear.
// FPT_WBITS = 17
#define FPT_WBITS 17

// 2. DEFINE saturation behavior (this is critical for control systems)
// This tells the library to use the "return FPT_MAX" handlers on overflow.
#define FPT_ADD_OVERFLOW_HANDLING
#define FPT_SUB_OVERFLOW_HANDLING
#define FPT_MUL_OVERFLOW_HANDLING
#define FPT_DIV_OVERFLOW_HANDLING

// 3. INCLUDE the library
// The preprocessor will now build the functions using our settings.
#include "fptc.h" 

// --- Simple Test Framework ---
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (condition) { \
            printf("[PASS] %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("[FAIL] %s\n", msg); \
            tests_failed++; \
        } \
    } while (0)

// Use 64-bit types for comparison and printing to handle 32-bit results
#define TEST_ASSERT_FPT_EQ(expected, actual, msg) \
    do { \
        if ((expected) == (actual)) { \
            printf("[PASS] %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("[FAIL] %s (Expected: %lld, Got: %lld)\n", msg, \
                   (int64_t)expected, (int64_t)actual); \
            tests_failed++; \
        } \
    } while (0)

// --- Test Cases ---

void test_conversions() {
    printf("--- Testing Conversions (Q16.15 format, FPT_FBITS = 15) ---\n");
    
    // FPT_ONE is 1 << 15 = 32768
    TEST_ASSERT_FPT_EQ(32768, FPT_ONE, "FPT_ONE is 32768");

    // 1.0f -> 32768
    TEST_ASSERT_FPT_EQ(32768, fl2fpt(1.0f), "Float 1.0 -> Fixed 32768");
    
    // 0.5f -> 16384 (rounding from 16384.0)
    TEST_ASSERT_FPT_EQ(16384, fl2fpt(0.5f), "Float 0.5 -> Fixed 16384");

    // 0.15f -> 0.15 * 32768 + 0.5 = 4915.2 + 0.5 = 4915.7 -> 4915
    TEST_ASSERT_FPT_EQ(4915, fl2fpt(0.15f), "Float 0.15 -> Fixed 4915");

    // Test small precision conversion
    // 0.0001f -> 0.0001 * 32768 + 0.5 = 3.2768 + 0.5 = 3.7768 -> 3
    TEST_ASSERT_FPT_EQ(3, fl2fpt(0.0001f), "Float 0.0001 -> Fixed 3");
    
    // -2 (int) -> -2 << 15 = -65536
    TEST_ASSERT_FPT_EQ(-65536, i2fpt(-2), "Int -2 -> Fixed -65536");

    // fpt2i(i2fpt(100)) == 100
    TEST_ASSERT_FPT_EQ(100, fpt2i(i2fpt(100)), "Int 100 round-trip");
}

void test_math() {
    printf("\n--- Testing Basic Math (Q16.15) ---\n");

    fpt a = fl2fpt(1.5f); // 1.5 * 32768 + 0.5 = 49152.5 -> 49152
    fpt b = fl2fpt(2.0f); // 2.0 * 32768 + 0.5 = 65536.5 -> 65536

    // 1.5 + 2.0 = 3.5 (3.5 * 32768 = 114688)
    TEST_ASSERT_FPT_EQ(114688, fpt_add(a, b), "Add 1.5 + 2.0 = 3.5");
    
    // 1.5 * 2.0 = 3.0 (fpt_mul truncates)
    // (49152 * 65536) >> 15 = 3221225472 >> 15 = 98304
    // 98304 is 3.0 * 32768
    TEST_ASSERT_FPT_EQ(98304, fpt_mul(a, b), "Mul 1.5 * 2.0 = 3.0");

    // 1.5 / 2.0 = 0.75
    // (49152 << 15) / 65536 = 1610612736 / 65536 = 24576
    // 24576 is 0.75 * 32768
    TEST_ASSERT_FPT_EQ(24576, fpt_div(a, b), "Div 1.5 / 2.0 = 0.75");

    printf("\n--- Testing Small Precision Math (Q16.15) ---\n");

    // --- Test 0.5 * 0.5 = 0.25 ---
    fpt half = fl2fpt(0.5f); // 0.5 * 32768 + 0.5 = 16384
    fpt quarter = fl2fpt(0.25f); // 0.25 * 32768 + 0.5 = 8192
    // (16384 * 16384) >> 15 = 268435456 >> 15 = 8192
    TEST_ASSERT_FPT_EQ(quarter, fpt_mul(half, half), "Mul 0.5 * 0.5 = 0.25");

    // --- Test 0.75 / 2.0 = 0.375 ---
    fpt p75 = fl2fpt(0.75f); // 0.75 * 32768 + 0.5 = 24576
    fpt p375 = fl2fpt(0.375f); // 0.375 * 32768 + 0.5 = 12288
    // (24576 << 15) / 65536 = 805306368 / 65536 = 12288
    TEST_ASSERT_FPT_EQ(p375, fpt_div(p75, b), "Div 0.75 / 2.0 = 0.375");

    // --- Test 0.1 * 0.1 = 0.01 (Precision/Truncation) ---
    fpt p1 = fl2fpt(0.1f); // 0.1 * 32768 + 0.5 = 3276.8 + 0.5 = 3277
    fpt p01_rounded = fl2fpt(0.01f); // 0.01 * 32768 + 0.5 = 327.68 + 0.5 = 328
    // (3277 * 3277) >> 15 = 10738729 >> 15 = 327 (fpt_mul truncates)
    TEST_ASSERT_FPT_EQ(327, fpt_mul(p1, p1), "Mul 0.1 * 0.1 = 0.01 (truncated)");
    TEST_ASSERT(p01_rounded == 328, "fl2fpt(0.01) rounds to 328");
}

void test_saturation() {
    printf("\n--- Testing Saturation (Control System Safety) ---\n");

    fpt max = FPT_MAX;      // 0x7FFFFFFF
    fpt min = FPT_MIN;      // 0x80000000
    fpt one = i2fpt(1);     // 32768
    fpt neg_one = i2fpt(-1); // -32768

    // 1. Test Add Saturation
    // We defined FPT_ADD_OVERFLOW_HANDLING, so this should clamp.
    fpt sat_res = fpt_add(max, one);
    TEST_ASSERT_FPT_EQ(FPT_MAX, sat_res, "Saturating Add clamps to MAX");

    // 2. Test Sub Saturation
    // We defined FPT_SUB_OVERFLOW_HANDLING, so this should clamp.
    sat_res = fpt_sub(min, one);
    TEST_ASSERT_FPT_EQ((FPT_MIN - 1), sat_res, "Saturating Sub clamps to MIN");

    // 3. Test Add Underflow
    sat_res = fpt_add(min, neg_one);
    TEST_ASSERT_FPT_EQ(FPT_MIN, sat_res, "Saturating Add clamps to MIN (underflow)");
}

void test_truncation() {
    printf("\n--- Testing Truncation ---\n");
    
    // This test shows that fpt_mul() truncates (rounds toward zero)
    //
    // 1.1 * 1.1 = 1.21
    //
    // fl2fpt(1.1f): 1.1 * 32768 + 0.5 = 36044.8 + 0.5 -> 36045.3 -> 36045
    fpt a = fl2fpt(1.1f); 
    
    // fpt_mul(a, a): (36045 * 36045) >> 15
    // = 1299242025 >> 15
    // = 39649.0... -> 39649 (truncated)
    fpt result = fpt_mul(a, a);
    TEST_ASSERT_FPT_EQ(39649, result, "fpt_mul(1.1, 1.1) truncates");

    // "Correct" rounded answer:
    // fl2fpt(1.21f): 1.21 * 32768 + 0.5 = 39649.28 + 0.5 -> 39649.78 -> 39650
    fpt rounded_ans = fl2fpt(1.21f);
    TEST_ASSERT_FPT_EQ(39650, rounded_ans, "fl2fpt(1.21) rounds up");

    // The truncated result is one bit less than the rounded result
    TEST_ASSERT(result == (rounded_ans - 1), "Truncated result is 1 LSB less than rounded");
}

int main() {
    printf("=== fptc Fixed Point Library Test Suite ===\n");
    printf("=== Format: Q16.15 (FPT_WBITS=17, FPT_FBITS=15)\n");
    printf("=== Saturation: ENABLED\n");
    
    test_conversions();
    test_math();
    test_saturation();
    test_truncation();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return (tests_failed == 0) ? 0 : 1;
}