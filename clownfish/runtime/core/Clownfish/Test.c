/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_CFISH_TESTBATCH
#define CFISH_USE_SHORT_NAMES
#define LUCY_USE_SHORT_NAMES
#define CHY_USE_SHORT_NAMES

#include "Clownfish/Test.h"
#include "Clownfish/VTable.h"

TestBatch*
TestBatch_new(int64_t num_tests) {
    TestBatch *self = (TestBatch*)VTable_Make_Obj(TESTBATCH);
    return TestBatch_init(self, num_tests);
}

TestBatch*
TestBatch_init(TestBatch *self, int64_t num_tests) {
    // Assign.
    self->num_tests       = num_tests;

    // Initialize.
    self->test_num        = 0;
    self->num_passed      = 0;
    self->num_failed      = 0;
    self->num_skipped     = 0;

    // Unbuffer stdout. TODO: move this elsewhere.
    int check_val = setvbuf(stdout, NULL, _IONBF, 0);
    if (check_val != 0) {
        fprintf(stderr, "Failed when trying to unbuffer stdout\n");
    }

    return self;
}

void
TestBatch_plan(TestBatch *self) {
    printf("1..%" PRId64 "\n", self->num_tests);
}

bool
TestBatch_test_true(void *vself, bool condition, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VTest_True((TestBatch*)vself, condition,
                                         pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_test_false(void *vself, bool condition, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VTest_False((TestBatch*)vself, condition,
                                          pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_test_int_equals(void *vself, long got, long expected,
                          const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VTest_Int_Equals((TestBatch*)vself, got,
                                               expected, pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_test_float_equals(void *vself, double got, double expected,
                            const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VTest_Float_Equals((TestBatch*)vself, got,
                                                 expected, pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_test_string_equals(void *vself, const char *got,
                             const char *expected, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VTest_String_Equals((TestBatch*)vself, got,
                                                  expected, pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_pass(void *vself, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VPass((TestBatch*)vself, pattern, args);
    va_end(args);
    return result;
}

bool
TestBatch_fail(void *vself, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    bool result = TestBatch_VFail((TestBatch*)vself, pattern, args);
    va_end(args);
    return result;
}

void
TestBatch_skip(void *vself, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    TestBatch_VSkip((TestBatch*)vself, pattern, args);
    va_end(args);
}

bool
TestBatch_vtest_true(TestBatch *self, bool condition, const char *pattern,
                     va_list args) {
    // Increment test number.
    self->test_num++;

    // Test condition and pass or fail.
    if (condition) {
        self->num_passed++;
        printf("ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return true;
    }
    else {
        self->num_failed++;
        printf("not ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return false;
    }
}

bool
TestBatch_vtest_false(TestBatch *self, bool condition,
                      const char *pattern, va_list args) {
    // Increment test number.
    self->test_num++;

    // Test condition and pass or fail.
    if (!condition) {
        self->num_passed++;
        printf("ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return true;
    }
    else {
        self->num_failed++;
        printf("not ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return false;
    }
}

bool
TestBatch_vtest_int_equals(TestBatch *self, long got, long expected,
                           const char *pattern, va_list args) {
    // Increment test number.
    self->test_num++;

    // Test condition and pass or fail.
    if (expected == got) {
        self->num_passed++;
        printf("ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return true;
    }
    else {
        self->num_failed++;
        printf("not ok %" PRId64 " - Expected '%ld', got '%ld'\n    ",
               self->test_num, expected, got);
        vprintf(pattern, args);
        printf("\n");
        return false;
    }
}

bool
TestBatch_vtest_float_equals(TestBatch *self, double got, double expected,
                             const char *pattern, va_list args) {
    double diff = expected / got;

    // Increment test number.
    self->test_num++;

    // Evaluate condition and pass or fail.
    if (diff > 0.00001) {
        self->num_passed++;
        printf("ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return true;
    }
    else {
        self->num_failed++;
        printf("not ok %" PRId64 " - Expected '%f', got '%f'\n    ",
               self->test_num, expected, got);
        vprintf(pattern, args);
        printf("\n");
        return false;
    }
}

bool
TestBatch_vtest_string_equals(TestBatch *self, const char *got,
                              const char *expected, const char *pattern,
                              va_list args) {
    // Increment test number.
    self->test_num++;

    // Test condition and pass or fail.
    if (strcmp(expected, got) == 0) {
        self->num_passed++;
        printf("ok %" PRId64 " - ", self->test_num);
        vprintf(pattern, args);
        printf("\n");
        return true;
    }
    else {
        self->num_failed++;
        printf("not ok %" PRId64 " - Expected '%s', got '%s'\n    ",
               self->test_num, expected, got);
        vprintf(pattern, args);
        printf("\n");
        return false;
    }
}

bool
TestBatch_vpass(TestBatch *self, const char *pattern, va_list args) {
    // Increment test number.
    self->test_num++;

    // Update counter, indicate pass.
    self->num_passed++;
    printf("ok %" PRId64 " - ", self->test_num);
    vprintf(pattern, args);
    printf("\n");

    return true;
}

bool
TestBatch_vfail(TestBatch *self, const char *pattern, va_list args) {
    // Increment test number.
    self->test_num++;

    // Update counter, indicate failure.
    self->num_failed++;
    printf("not ok %" PRId64 " - ", self->test_num);
    vprintf(pattern, args);
    printf("\n");

    return false;
}

void
TestBatch_vskip(TestBatch *self, const char *pattern, va_list args) {
    self->test_num++;
    printf("ok %" PRId64 " # SKIP ", self->test_num);
    vprintf(pattern, args);
    printf("\n");
    self->num_skipped++;
}


