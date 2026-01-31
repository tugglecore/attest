/* Copyright (c) 2026 Demetrius Tuggle
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************
 * OPTIONS
 *************************/
// Default max tests for a single executable
#ifndef ATTEST_MAX_TESTS
#define ATTEST_MAX_TESTS 128
#endif

#ifndef ATTEST_CASE_NAME_SIZE
#define ATTEST_CASE_NAME_SIZE 128
#endif
//
// Default max failures for a parameterize test
#ifndef ATTEST_MAX_PARAMERTERIZE_RESULTS
#define ATTEST_MAX_PARAMERTERIZE_RESULTS 32
#endif

// Max buffer size to represent values
#ifndef ATTEST_VALUE_BUF
#define ATTEST_VALUE_BUF 128
#endif

// Max amount of attempts
#ifndef ATTEST_MAX_TEST_ATTEMPTS
#define ATTEST_MAX_TEST_ATTEMPTS 32
#endif
//
// Max amount of attempts
#ifndef ATTEST_MAX_FAILURES
#define ATTEST_MAX_FAILURES 16
#endif

#ifdef ATTEST_NO_COLOR
#define RED ""
#define NORMAL ""
#define GREEN ""
#define MAGENTA ""
#define CYAN ""
#define YELLOW ""
#define GRAY ""
#define BOLD_WHITE ""
#else
#define RED "\x1b[31m"
#define NORMAL "\x1b[0m"
#define GREEN "\x1b[32m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define YELLOW "\x1b[33m"
#define GRAY "\x1b[2m"
#define BOLD_WHITE "\x1b[1;97m"
#endif

#ifdef ATTEST_NO_UTF8
#define BRANCH "|--"
#define TRUNK "|"
#define LEAF "|__"
#else
#define BRANCH "├──"
#define TRUNK "│"
#define LEAF "└──"
#endif
/**************************
 * PUBLIC TYPES
 *************************/

typedef struct
{
    void* shared;
    void* local;
} TestContext;

typedef struct
{
    void* shared;
    void* set;
    void* local;
    void* case_data;
    char* case_name;
} ParamContext;

/**************************
 * PRIVATE TYPES
 *************************/

typedef enum {
    MISSING_EXPECTATION,
    PASSED,
    FAILED,
} Status;

typedef struct TestConfig {
    char* filename;
    int line;
    char* test_title;
    bool skip;
    bool disabled;
    int attempts;
    void (*contextual_test)(TestContext*);
    void (*simple_test)(void);
    void (*before)(TestContext*);
    void (*after)(TestContext*);
    void (*before_all_cases)(ParamContext*);
    void (*before_each_case)(ParamContext*);
    void (*after_all_cases)(ParamContext*);
    void (*after_each_case)(ParamContext*);
    void (*param_init)(void);
    void (*param_test_runner)(void);
    void (*param_test)(struct TestConfig);
    struct TestConfig* next;
    int attempt_count;
    int param_index;
    char* tags[10];
    Status status;
} TestConfig;

typedef struct
{
    char* title;
    bool skip;
    void* data;
} TestCase;

typedef struct
{
    char* test_name;
    char* filename;
    int line;
    bool has_msg;
    char msg[ATTEST_VALUE_BUF];
    char verification_text[ATTEST_VALUE_BUF];
    char actual_label[ATTEST_VALUE_BUF];
    char actual_value[ATTEST_VALUE_BUF];
    bool has_expected_value;
    char expected_label[ATTEST_VALUE_BUF];
    char expected_value[ATTEST_VALUE_BUF];
    char reason[ATTEST_VALUE_BUF];
} FailureInfo;

typedef struct
{
    FailureInfo failures[ATTEST_MAX_FAILURES];
    size_t count;
} FailureList;

// TODO: standarize instance noun to case
typedef struct
{
    char* case_name;
    Status status;
    int failure_count;
    FailureInfo failures[ATTEST_MAX_PARAMERTERIZE_RESULTS];
    bool has_status;
} InstanceResult;

typedef struct
{
    void* shared;
} GlobalContext;

typedef struct
{
    void* global_shared_data;
} AttestContext;

void display_failures(int test_attempt, char* failure_report_preamble);
void report_summary();
bool has_status(Status target_status, const Status* statuses, int status_count);
bool any_instance(Status status);
bool every_instance(Status status);

/**************************
 * GLOBALS
 *************************/
int total_tests = 0;
int pass_count = 0;
int fail_count = 0;
int skip_count = 0;
int empty_count = 0;
static int case_count = 0;

static TestConfig* attest_registry_head = NULL;

FailureList failed_assertions_per_attempt[ATTEST_MAX_TEST_ATTEMPTS];
// FailureInfo failed_verifications[ATTEST_MAX_TESTS];
int test_attempt_count = 0;
// int failed_verification_count = 0;

InstanceResult parameterize_instance_results[ATTEST_MAX_TESTS];

static void (*parameterize_before_all_cases)(ParamContext* param_ctx);
static void (*parameterize_after_all_cases)(ParamContext* param_ctx);

TestConfig* attest_internal_current_test;

static void (*attest_before_each_handler)(TestContext* test_ctx);
static void (*attest_before_all_handler)(GlobalContext* global_ctx);

static void (*attest_after_all_handler)(GlobalContext* global_ctx);

static void (*attest_after_each_handler)(TestContext* global_ctx);

static AttestContext attest_context = {
    .global_shared_data = NULL
};

static ParamContext global_param_context;

/**************************
 * ENGINES
 *************************/

void attest_update_registry(TestConfig* test_config)
{
    if (attest_registry_head == NULL) {
        test_config->next = NULL;
        attest_registry_head = test_config;
        return;
    }

    TestConfig* pointer = attest_registry_head;

    while (pointer->next != NULL) {
        pointer = pointer->next;
    }

    pointer->next = test_config;
    test_config->next = NULL;
}

void attester(TestConfig cfg)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
    attest_internal_current_test = &cfg;
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop

    bool is_param_test = cfg.param_test != NULL;

    if (cfg.skip) {
        skip_count += 1;
        return;
    }

    test_attempt_count = 0;
    int max_attempts = cfg.attempts ? cfg.attempts : 1;
    Status statuses[ATTEST_MAX_TEST_ATTEMPTS];

    for (
        cfg.attempt_count = 0;
        cfg.attempt_count < max_attempts;
        cfg.attempt_count++) {
        TestContext context = {
            .shared = NULL,
            .local = NULL
        };

        global_param_context.local = NULL;

        if (attest_context.global_shared_data != NULL) {
            context.shared = attest_context.global_shared_data;
        }

        if (attest_before_each_handler != NULL && !is_param_test) {
            attest_before_each_handler(&context);
        }

        if (is_param_test && cfg.before) {
            (void)fprintf(stderr, "%s[ERROR] Use `before_each_case` instead of `before_each` for paramerterize tests.%s\n", RED, NORMAL);
            exit(1);
        }

        if (cfg.before) {
            cfg.before(&context);
        }

        if (cfg.before_each_case) {
            cfg.before_each_case(&global_param_context);
        }

        if (cfg.contextual_test) {
            cfg.contextual_test(&context);
        } else if (cfg.simple_test) {
            cfg.simple_test();
        } else if (cfg.param_test) {
            cfg.param_test(cfg);
        } else {
            (void)fprintf(stderr, "%s[ERROR] Attest entered invalid state. capture debug logs and file issue.%s\n", RED, NORMAL);
            exit(1);
        }

        if (is_param_test && cfg.after) {
            (void)fprintf(stderr, "%s[ERROR] Use `after_each_case` instead of `after_each` for paramerterize tests.%s\n", RED, NORMAL);
            exit(1);
        }

        if (cfg.after) {
            cfg.after(&context);
        }

        free(context.local);

        if (cfg.after_each_case) {
            cfg.after_each_case(&global_param_context);
        }

        if (attest_after_each_handler != NULL) {
            attest_after_each_handler(&context);
        }

        statuses[test_attempt_count] = cfg.status;
        test_attempt_count++;
        if (cfg.status == PASSED || cfg.status == MISSING_EXPECTATION) {
            break;
        }
    }

    if (test_attempt_count > max_attempts) {
        fprintf(stderr, "%s[ATTEST ERROR] Reach invalid state concerning test attempts. Print debug logs and file issue.%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    if (is_param_test && cfg.status == MISSING_EXPECTATION) {
        InstanceResult instance_result = { .status = MISSING_EXPECTATION };
        parameterize_instance_results[cfg.param_index] = instance_result;
    }

    if (is_param_test) {
        return;
    }

    bool an_attempt_is_missing_verification = has_status(
        MISSING_EXPECTATION,
        statuses,
        test_attempt_count);

    if (an_attempt_is_missing_verification) {
        cfg.status = MISSING_EXPECTATION;
    }

    switch (cfg.status) {
    case PASSED:
        pass_count++;
        break;
    case MISSING_EXPECTATION:
        empty_count++;
        printf(
            "%s[MISSING ASSERTION]%s %s%s%s\n",
            MAGENTA,
            NORMAL,
            BOLD_WHITE,
            attest_internal_current_test->test_title,
            NORMAL);
        printf("%s Location:%s %s%s:%d%s\n\n",
            CYAN, NORMAL, GRAY, cfg.filename,
            cfg.line,
            NORMAL);
        break;
    case FAILED:
        fail_count++;
        printf(
            "%s[FAIL]%s %s%s%s\n",
            RED, NORMAL, BOLD_WHITE,
            cfg.test_title,
            NORMAL);
        bool more_than_one_attempt = test_attempt_count > 1;
        for (int i = 0; i < test_attempt_count; i++) {
            bool is_last_attempt = i == test_attempt_count - 1;

            if (more_than_one_attempt) {
                printf("%s%s%sTest attempt: %d\n",
                    GRAY,
                    is_last_attempt ? LEAF : BRANCH,
                    NORMAL,
                    i + 1);
            }

            if (more_than_one_attempt) {
                display_failures(
                    i,
                    is_last_attempt
                        ? "   "
                        : TRUNK "   ");
            } else {
                display_failures(i, "");
            }
        }
        break;
    default:
        fprintf(stderr, "%s[ATTEST ERROR] Reach unreachable state. Print debug logs and file issue%s\n", RED, NORMAL);
    }
}

void run_parameterize_test(TestConfig* test_config)
{
    test_config->param_init();

    if (case_count == 0) {
        fprintf(stderr, "%s[ATTEST ERROR] Pass values enclosed with parenthesis when using `PARAM_TEST` or `PARAM_TEST_CTX`.%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    global_param_context.shared = attest_context.global_shared_data;

    if (parameterize_before_all_cases != NULL) {
        parameterize_before_all_cases(&global_param_context);
    }

    test_config->param_test_runner();

    bool empty_tests_are_present = any_instance(MISSING_EXPECTATION);

    bool every_instance_pass = every_instance(PASSED);

    if (every_instance_pass) {
        pass_count++;
    } else if (empty_tests_are_present) {
        empty_count++;
        printf(
            "%s[MISSING ASSERTION]%s %s%s%s\n",
            MAGENTA,
            NORMAL,
            BOLD_WHITE, test_config->test_title, NORMAL);
        printf(
            "%s NOTE:%s Every case of a pareametize test must have atleast one expectation.\n",
            CYAN, NORMAL);
        printf("%s Location:%s %s%s:%d%s\n\n",
            CYAN, NORMAL, GRAY, test_config->filename,
            test_config->line, NORMAL);
    } else {
        fail_count++;

        int amount_of_passed_cases = 0;
        int amount_of_failed_cases = 0;
        InstanceResult* failed_cases[ATTEST_MAX_TESTS];

        for (int i = 0; i < case_count; i++) {
            switch (parameterize_instance_results[i].status) {
            case PASSED:
                amount_of_passed_cases++;
                break;
            case FAILED:
                failed_cases[amount_of_failed_cases] = &parameterize_instance_results[i];
                amount_of_failed_cases++;
                break;
            case MISSING_EXPECTATION:
                fprintf(
                    stderr,
                    "%s[ERROR] Missing Assertion in parameterize test case. Please use atleast one expectation in every case. Location: %s:%d%s\n",
                    RED,
                    test_config->filename,
                    test_config->line,
                    NORMAL);
                exit(1); // NOLINT
                break;
            default:
                fprintf(
                    stderr,
                    "%s[ERROR] Reached unreachable state. File issue%s\n",
                    RED,
                    NORMAL);
                exit(1); // NOLINT
            }
        }

        printf(
            "%s[FAIL]%s %s%s%s (%d/%d passed)\n",
            RED, NORMAL, BOLD_WHITE,
            test_config->test_title,
            NORMAL,
            amount_of_passed_cases, case_count);

        printf("%s%s%s\n", GRAY, TRUNK, NORMAL);

        for (int i = 0; i < amount_of_failed_cases; i++) {
            bool is_last_failed_case = i == amount_of_failed_cases - 1;
            InstanceResult* case_result = failed_cases[i];

            printf(
                "%s%s Case [%d]:%s %s\n",
                GRAY,
                is_last_failed_case ? LEAF : BRANCH,
                i + 1,
                NORMAL,
                case_result->case_name[0] == '\0'
                    ? "<unnamed>"
                    : case_result->case_name);

            for (int j = 0; j < case_result->failure_count; j++) {
                bool is_last_failure = j == case_result->failure_count - 1;

                if (is_last_failed_case) {
                    printf("    ");
                } else {
                    printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                }

                FailureInfo case_failure_info = case_result->failures[j];
                printf(
                    "%s%s%s %s@L%d: %s\n",
                    GRAY,
                    is_last_failure ? LEAF : BRANCH,
                    NORMAL,
                    case_failure_info.filename,
                    case_failure_info.line,
                    case_failure_info.verification_text);

                if (case_failure_info.has_msg) {
                    if (is_last_failed_case) {
                        printf("    ");
                    } else {
                        printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                    }

                    if (is_last_failure) {
                        printf("    ");
                    } else {
                        printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                    }

                    printf("Message: %s\n", case_failure_info.msg);
                }

                if (case_failure_info.has_expected_value) {
                    if (is_last_failed_case) {
                        printf("    ");
                    } else {
                        printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                    }

                    if (is_last_failure) {
                        printf("    ");
                    } else {
                        printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                    }

                    printf("Expected: %s%s%s\n", GREEN, case_failure_info.expected_value, NORMAL);
                }

                if (is_last_failed_case) {
                    printf("    ");
                } else {
                    printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                }

                if (is_last_failure) {
                    printf("    ");
                } else {
                    printf("%s%s%s   ", GRAY, TRUNK, NORMAL);
                }

                printf("Actual:   %s%s%s\n", RED, case_failure_info.actual_value, NORMAL);
            }
        }
    }

    printf("\n");

    if (parameterize_after_all_cases != NULL) {
        parameterize_after_all_cases(&global_param_context);
    }

    global_param_context.shared = NULL;
    global_param_context.set = NULL;
    global_param_context.local = NULL;

    parameterize_before_all_cases = NULL;
    parameterize_after_all_cases = NULL;
    case_count = 0;

    memset(parameterize_instance_results, 0, sizeof(InstanceResult) * ATTEST_MAX_TESTS);
}

int main(void)
{
    char* test_titles[ATTEST_MAX_TESTS];
    int test_count = 0;
    TestConfig* test_config = attest_registry_head;

    while (test_config) {
        if (test_count == ATTEST_MAX_TESTS) {
            fprintf(stderr,
                "[ERROR] Reached max allowed tests. Define MACRO "
                "ATTEST_MAX_TESTS to higher limit");
            exit(1);
        }

        if (test_config->attempts < 0) {
            fprintf(
                stderr,
                "%s[ERROR] `attempts` need to be positive. Location: %s:%d%s\n",
                RED,
                test_config->filename,
                test_config->line,
                NORMAL);
            exit(1); // NOLINT
        }

        if (test_config->attempts > ATTEST_MAX_TEST_ATTEMPTS) {
            fprintf(
                stderr,
                "%s[ERROR] `attempts` need to be less than or equal to %d. Location: %s:%d%s\n",
                RED,
                ATTEST_MAX_TEST_ATTEMPTS,
                test_config->filename,
                test_config->line,
                NORMAL);
            exit(1); // NOLINT
        }

        if (test_config->test_title == NULL) {
            fprintf(
                stderr,
                "%s[ERROR] Test case missing title. Location: %s:%d%s\n",
                RED,
                test_config->filename,
                test_config->line,
                NORMAL);
            exit(1); // NOLINT
        }

        if (test_config->param_test == NULL) {
            for (int i = 0; i < test_count; i++) {
                char* past_test_title = test_titles[i];
                bool is_duplicate = strcmp(test_config->test_title, past_test_title) == 0;
                if (is_duplicate) {
                    fprintf(
                        stderr,
                        "%s[ERROR] Duplicate Test case title. Location: %s:%d%s\n",
                        RED,
                        test_config->filename,
                        test_config->line,
                        NORMAL);
                    exit(1); // NOLINT
                }
            }
        }

        test_titles[test_count] = test_config->test_title;
        test_count++;
        test_config = test_config->next;
    }

    GlobalContext global_context = { .shared = NULL };

    if (attest_before_all_handler) {
        attest_before_all_handler(&global_context);

        if (global_context.shared != NULL) {
            attest_context.global_shared_data = global_context.shared;
        }
    }

    test_config = attest_registry_head;

    // TODO: Discover behavior of a parameterize test with > 1 attempts sets.
    while (test_config) {
        if (test_config->disabled) {
            test_config = test_config->next;
            continue;
        }

        if (test_config->param_test_runner) {
            run_parameterize_test(test_config);
        } else {
            attester(*test_config);
        }

        memset(failed_assertions_per_attempt, 0, sizeof failed_assertions_per_attempt);
        // failed_verification_count = 0;

        total_tests++;

        test_config = test_config->next;
    }

    if (attest_after_all_handler) {
        attest_after_all_handler(&global_context);
    }

    report_summary();

    return 0;
}

/**************************
 * REPORTERS
 *************************/
void display_failures(int test_attempt, char* failure_report_preamble)
{
    FailureList* failure_list = &failed_assertions_per_attempt[test_attempt];

    for (size_t i = 0; i < failure_list->count; i++) {
        FailureInfo* failure_info = &failure_list->failures[i];
        bool is_last_failed_verification = i == failure_list->count - 1;
        char* verification_preamble = is_last_failed_verification ? LEAF : BRANCH;

        printf("%s%s%s%s %s@L%d: %s\n",
            GRAY, failure_report_preamble, verification_preamble, NORMAL,
            failure_info->filename, failure_info->line,
            failure_info->verification_text);

        char detail_preamble[7];
        snprintf(detail_preamble, 7, "%s",
            is_last_failed_verification ? "    " : TRUNK "  ");

        bool actual_has_label = strcmp(failure_info->actual_label, failure_info->actual_value) != 0;
        if (failure_info->has_expected_value) {

            bool expected_has_label = strcmp(failure_info->expected_label, failure_info->expected_value) != 0;

            if (actual_has_label) {
                printf("%s%s%s%s %s = %s\n",
                    GRAY, failure_report_preamble, detail_preamble, NORMAL,
                    failure_info->actual_label, failure_info->actual_value);
            }

            if (expected_has_label) {
                printf("%s%s%s%s %s = %s\n",
                    GRAY, failure_report_preamble, detail_preamble, NORMAL,
                    failure_info->expected_label, failure_info->expected_value);
            }
        } else {
            if (actual_has_label) {
                printf("%s%s%s%sActual: %s = %s\n",
                    GRAY, failure_report_preamble, detail_preamble, NORMAL,
                    failure_info->actual_label, failure_info->actual_value);
            }

            printf("%s%s%s%sReason: %s\n",
                GRAY, failure_report_preamble, detail_preamble, NORMAL,
                failure_info->reason);
        }

        if (failure_info->has_msg) {
            printf("%s%s%s%sMessage: %s\n",
                GRAY, failure_report_preamble, detail_preamble, NORMAL,
                failure_info->msg);
        }

        printf("%s%s%s%s\n", GRAY, failure_report_preamble, detail_preamble, NORMAL);
    }
}

void report_summary()
{
    printf("%s==============Test Summary==============%s\n", MAGENTA, NORMAL);
    printf("%s  Total:          %d%s\n", MAGENTA, total_tests, NORMAL);
    printf("%s  Passed:         %d%s\n", GREEN, pass_count, NORMAL);
    printf("%s  Skipped:        %d%s\n", YELLOW, skip_count, NORMAL);
    printf("%s  Failed:         %d%s\n", RED, fail_count, NORMAL);

    if (empty_count) {
        printf("%s  No assertions:  %d%s\n", CYAN, empty_count, NORMAL);
    }

    exit(fail_count || empty_count ? 1 : 0); // NOLINT
}

void report_success()
{
    TestConfig* current_test = attest_internal_current_test;

    current_test->status = PASSED;

    if (current_test->attempts > 0) {
        printf(
            "%s ->%s %sAttempt %d:%s %sPassed%s\n",
            GRAY, NORMAL, CYAN,
            current_test->attempt_count + 1,
            NORMAL,
            GREEN,
            NORMAL);
    } else if (current_test->param_test && !parameterize_instance_results[current_test->param_index].has_status) {
        parameterize_instance_results[current_test->param_index] = (InstanceResult) {
            .case_name = global_param_context.case_name,
            .has_status = true,
            .status = PASSED
        };
    }
}

void report_failed_attempt(FailureInfo failure_info)
{
    TestConfig* current_test = attest_internal_current_test;

    if (current_test->attempt_count < current_test->attempts) {
        printf(
            "%s -> %s%sAttempt %d: %sFailed%s %s(Assertion at line %d)%s\n",
            GRAY, NORMAL, CYAN,
            current_test->attempt_count + 1,
            RED, NORMAL, GRAY,
            failure_info.line,
            NORMAL);
    }

    if (current_test->attempt_count == current_test->attempts - 1) {
        printf("\n");
    }
}

void report_failure(FailureInfo failure_info)
{
    attest_internal_current_test->status = FAILED;

    if (attest_internal_current_test->param_test) {
        InstanceResult* case_result = &parameterize_instance_results[attest_internal_current_test->param_index];
        case_result->has_status = true;
        case_result->case_name = global_param_context.case_name;
        case_result->status = FAILED;
        case_result->failures[case_result->failure_count] = failure_info;
        case_result->failure_count++;
    } else {
        FailureList* failure_list = &failed_assertions_per_attempt[test_attempt_count];
        failure_list->failures[failure_list->count] = failure_info;
        failure_list->count++;
    }
}

/**************************
 * UTILITIES
 *************************/

bool has_status(Status target_status, const Status* statuses, int status_count)
{
    for (int i = 0; i < status_count; i++) {
        if (statuses[i] == target_status) {
            return true;
        }
    }
    return false;
}

bool any_instance(Status status)
{
    for (int i = 0; i < case_count; i++) {
        if (parameterize_instance_results[i].status == status) {
            return true;
        }
    }
    return false;
}

bool every_instance(Status status)
{
    for (int i = 0; i < case_count; i++) {
        if (parameterize_instance_results[i].status != status) {
            return false;
        }
    }
    return true;
}

/**************************
 * MACROS
 *************************/

#define TAGS(...) .tags = { __VA_ARGS__, NULL }

#define TEST(name, ...)                                            \
    static void name(void);                                        \
    static void __attribute__((constructor)) register_##name(void) \
    {                                                              \
        static TestConfig test_config = {                          \
            .filename = __FILE__,                                  \
            .line = __LINE__,                                      \
            .test_title = #name,                                   \
            .simple_test = name,                                   \
            __VA_ARGS__                                            \
        };                                                         \
        attest_update_registry(&test_config);                      \
    }                                                              \
    static void name(void)

#define TEST_CTX(name, ctx, ...)                                   \
    static void name(TestContext* ctx);                            \
    static void __attribute__((constructor)) register_##name(void) \
    {                                                              \
        static TestConfig test_config = {                          \
            .filename = __FILE__,                                  \
            .line = __LINE__,                                      \
            .test_title = #name,                                   \
            .contextual_test = name,                               \
            .skip = false,                                         \
            __VA_ARGS__                                            \
        };                                                         \
        attest_update_registry(&test_config);                      \
    }                                                              \
    static void name(TestContext* ctx)

#define BEFORE_ALL(context)                                                   \
    static void attest_before_all(GlobalContext*(context));                   \
    static void __attribute__((constructor)) register_attest_before_all(void) \
    {                                                                         \
        attest_before_all_handler = attest_before_all;                        \
    }                                                                         \
    static void attest_before_all(GlobalContext*(context))

#define BEFORE_EACH(context)                                                   \
    static void attest_before_each(TestContext*(context));                     \
    static void __attribute__((constructor)) register_attest_before_each(void) \
    {                                                                          \
        attest_before_each_handler = attest_before_each;                       \
    }                                                                          \
    static void attest_before_each(TestContext*(context))

#define AFTER_ALL(ctx)                                                       \
    static void attest_after_all(GlobalContext*(ctx));                       \
    static void __attribute__((constructor)) register_attest_after_all(void) \
    {                                                                        \
        attest_after_all_handler = attest_after_all;                         \
    }                                                                        \
    static void attest_after_all(GlobalContext*(ctx))

#define AFTER_EACH(context)                                                   \
    static void attest_after_each(TestContext*(context));                     \
    static void __attribute__((constructor)) register_attest_after_each(void) \
    {                                                                         \
        attest_after_each_handler = attest_after_each;                        \
    }                                                                         \
    static void attest_after_each(TestContext*(context))

#define UNWRAP(...) __VA_ARGS__
#define STRIP_PARENS(x) UNWRAP x

#define PARAM_TEST(title, param_type, param_var, values_group, ...)          \
    void title##_impl(param_type param_var);                                 \
    void title##_impl_wrapper(TestConfig cfg);                               \
    struct title##_type {                                                    \
        param_type data;                                                     \
        char name[ATTEST_CASE_NAME_SIZE];                                    \
    };                                                                       \
    static struct title##_type title##_data[] = {                            \
        STRIP_PARENS(values_group)                                           \
    };                                                                       \
                                                                             \
    void title##_init(void)                                                  \
    {                                                                        \
        TestConfig cfg = { __VA_ARGS__ };                                    \
        case_count = sizeof(title##_data) / sizeof(struct title##_type);     \
        parameterize_before_all_cases = cfg.before_all_cases;                \
        parameterize_after_all_cases = cfg.after_all_cases;                  \
    }                                                                        \
    void title##_runner(void)                                                \
    {                                                                        \
        for (int i = 0; i < case_count; i++) {                               \
            TestConfig param_cfg = {                                         \
                .filename = __FILE__,                                        \
                .line = __LINE__,                                            \
                .test_title = #title,                                        \
                .param_test = title##_impl_wrapper,                          \
                .param_index = i,                                            \
                __VA_ARGS__                                                  \
            };                                                               \
            struct title##_type* test_case = &title##_data[i];               \
            global_param_context.case_data = (void*)&test_case->data;        \
            global_param_context.case_name = test_case->name;                \
            attester(param_cfg);                                             \
        }                                                                    \
    }                                                                        \
    static void __attribute__((constructor)) register_##title##_runner(void) \
    {                                                                        \
        static TestConfig test_config = {                                    \
            .filename = __FILE__,                                            \
            .line = __LINE__,                                                \
            .test_title = #title,                                            \
            .param_test_runner = title##_runner,                             \
            .param_init = title##_init,                                      \
        };                                                                   \
        attest_update_registry(&test_config);                                \
    }                                                                        \
    void title##_impl_wrapper(TestConfig cfg)                                \
    {                                                                        \
        title##_impl(title##_data[cfg.param_index].data);                    \
    }                                                                        \
    void title##_impl(param_type param_var)

#define PARAM_TEST_CTX(title,                                                    \
    context, param_type, param_var, values_group, ...)                           \
    void title##_impl(ParamContext* context, param_type param_var);              \
    void title##_impl_wrapper(TestConfig cfg);                                   \
    struct title##_type {                                                        \
        param_type data;                                                         \
        char name[ATTEST_CASE_NAME_SIZE];                                        \
    };                                                                           \
    static struct title##_type title##_data[] = {                                \
        STRIP_PARENS(values_group)                                               \
    };                                                                           \
                                                                                 \
    void title##_init(void)                                                      \
    {                                                                            \
        TestConfig cfg = { __VA_ARGS__ };                                        \
        case_count = sizeof(title##_data) / sizeof(struct title##_type);         \
        parameterize_before_all_cases = cfg.before_all_cases;                    \
        parameterize_after_all_cases = cfg.after_all_cases;                      \
    }                                                                            \
    void title##_runner(void)                                                    \
    {                                                                            \
        for (int i = 0; i < case_count; i++) {                                   \
            TestConfig param_cfg = {                                             \
                .filename = __FILE__,                                            \
                .line = __LINE__,                                                \
                .test_title = #title,                                            \
                .param_test = title##_impl_wrapper,                              \
                .param_index = i,                                                \
                __VA_ARGS__                                                      \
            };                                                                   \
            struct title##_type* test_case = &title##_data[i];                   \
            global_param_context.case_data = (void*)&test_case->data;            \
            global_param_context.case_name = test_case->name;                    \
            attester(param_cfg);                                                 \
        }                                                                        \
    }                                                                            \
    static void __attribute__((constructor)) register_##title##_runner(void)     \
    {                                                                            \
        static TestConfig test_config = {                                        \
            .filename = __FILE__,                                                \
            .line = __LINE__,                                                    \
            .test_title = #title,                                                \
            .param_test_runner = title##_runner,                                 \
            .param_init = title##_init,                                          \
        };                                                                       \
        attest_update_registry(&test_config);                                    \
    }                                                                            \
    void title##_impl_wrapper(TestConfig cfg)                                    \
    {                                                                            \
        /* TODO: pass a copy of global_param_context */                          \
        title##_impl(&global_param_context, title##_data[cfg.param_index].data); \
    }                                                                            \
    void title##_impl(ParamContext* context, param_type param_var)

/**************************
 * EXPECTATIONS
 *************************/
// TODO: We are voiding the return values of `sn?printf` to silence
// the warning from clang-tidy check `cert-err3-c`. Decide if it is
// worth the increase in macro compelxity to properly handle result.
#define PICK_ONE_FOR_CONDITION(x, ...) x

#define COUNT_ARG(                          \
    _01, _02, _03, _04, _05, _06, _07, _08, \
    _09, _10, _11, _12, _13, _14, _15, _16, \
    x, ...) x
#define DETERMINE_ACTION(...)                           \
    COUNT_ARG(__VA_ARGS__,                              \
        SAVE, SAVE, SAVE, SAVE, SAVE, SAVE, SAVE, SAVE, \
        SAVE, SAVE, SAVE, SAVE, SAVE, SAVE, SAVE, IGNORE)

#define CONCAT(a, b) a##b
#define EXPECT_DISPATCH_HELPER(action) CONCAT(action, _MESSAGE)
#define EXPECT_DISPATCH(...) EXPECT_DISPATCH_HELPER(DETERMINE_ACTION(__VA_ARGS__))

#define MSG_DISPATCH_FOR_ONE_ARG(...) \
    EXPECT_DISPATCH(__VA_ARGS__)(TAKE_ONE(__VA_ARGS__))

#define MSG_DISPATCH_FOR_TWO_ARGS(...) \
    EXPECT_DISPATCH(TAKE_ONE(__VA_ARGS__))(TAKE_TWO(__VA_ARGS__))

#define IGNORE_MESSAGE(...)

#define SAVE_MESSAGE(...)                                   \
    int msg_size = snprintf(NULL, 0, __VA_ARGS__);          \
    if (msg_size > 0) {                                     \
        failure_info.has_msg = true;                        \
        if (msg_size < ATTEST_VALUE_BUF) {                  \
            (void)snprintf(                                 \
                failure_info.msg,                           \
                ATTEST_VALUE_BUF,                           \
                __VA_ARGS__);                               \
        } else {                                            \
            (void)sprintf(failure_info.msg, "(truncated)"); \
        }                                                   \
    } else if (msg_size < 0) {                              \
        failure_info.has_msg = true;                        \
        (void)sprintf(failure_info.msg,                     \
            "[ERROR] Unable to format message");            \
    }

#define TAKE_ONE(x, ...) __VA_ARGS__
#define TAKE_TWO(x, y, ...) __VA_ARGS__

#define BUILD_RELATION_REASON(relation, x, y, ...) \
    "[ %s %s %s ]", #x, #relation, #y

#define BUILD_CONDITION_REASON(type, x, ...) \
    "[ %s ] should be %s", #x, #type

#define BUILD_RELATION(operator, x, y, ...) x operator y

#define UNGROUP(...) __VA_ARGS__

#define COLLECT_ONE_VERIFICATION_TOKEN(verification, x, ...) \
    int verification_token_size = snprintf(                  \
        NULL,                                                \
        0,                                                   \
        "%s(%s)",                                            \
        #verification, #x);                                  \
    if (verification_token_size > 0) {                       \
        if (verification_token_size < ATTEST_VALUE_BUF) {    \
            (void)snprintf(failure_info.verification_text,   \
                ATTEST_VALUE_BUF,                            \
                "%s(%s)",                                    \
                #verification, #x);                          \
        } else {                                             \
            (void)snprintf(failure_info.verification_text,   \
                ATTEST_VALUE_BUF,                            \
                "%s( truncated )",                           \
                #verification);                              \
        }                                                    \
    }

#define COLLECT_TWO_VERIFICATION_TOKENS(verification, x, y, ...) \
    int verification_token_size = snprintf(                      \
        NULL,                                                    \
        0,                                                       \
        "%s(%s, %s)",                                            \
        #verification, #x, #y);                                  \
    if (verification_token_size > 0) {                           \
        if (verification_token_size < ATTEST_VALUE_BUF) {        \
            (void)snprintf(failure_info.verification_text,       \
                ATTEST_VALUE_BUF,                                \
                "%s(%s, %s)",                                    \
                #verification, #x, #y);                          \
        } else {                                                 \
            (void)snprintf(failure_info.verification_text,       \
                ATTEST_VALUE_BUF,                                \
                "%s( truncated )",                               \
                #verification);                                  \
        }                                                        \
    }

#define SAVE_ONE_VALUE(format, failure_reason, x, ...) \
    failure_info.has_expected_value = false;           \
    int actual_label_size = snprintf(                  \
        NULL,                                          \
        0,                                             \
        "%s",                                          \
        #x);                                           \
    if (actual_label_size > 0) {                       \
        if (actual_label_size < ATTEST_VALUE_BUF) {    \
            (void)snprintf(failure_info.actual_label,  \
                ATTEST_VALUE_BUF,                      \
                "%s",                                  \
                #x);                                   \
        } else {                                       \
            (void)sprintf(failure_info.actual_label,   \
                "(truncated)");                        \
        }                                              \
    }                                                  \
    int actual_value_size = snprintf(                  \
        NULL,                                          \
        0,                                             \
        UNGROUP format(x));                            \
    if (actual_value_size > 0) {                       \
        if (actual_value_size < ATTEST_VALUE_BUF) {    \
            (void)snprintf(failure_info.actual_value,  \
                ATTEST_VALUE_BUF,                      \
                UNGROUP format(x));                    \
        } else {                                       \
            (void)sprintf(failure_info.actual_value,   \
                "(truncated)");                        \
        }                                              \
    }                                                  \
    (void)snprintf(failure_info.reason,                \
        ATTEST_VALUE_BUF,                              \
        failure_reason);

#define SAVE_TWO_VALUE(format, x, y, ...)               \
    failure_info.has_expected_value = true;             \
    int actual_label_size = snprintf(                   \
        NULL,                                           \
        0,                                              \
        "%s",                                           \
        #x);                                            \
    if (actual_label_size > 0) {                        \
        if (actual_label_size < ATTEST_VALUE_BUF) {     \
            (void)snprintf(failure_info.actual_label,   \
                ATTEST_VALUE_BUF,                       \
                "%s",                                   \
                #x);                                    \
        } else {                                        \
            (void)sprintf(failure_info.actual_label,    \
                "(truncated)");                         \
        }                                               \
    }                                                   \
    int actual_value_size = snprintf(                   \
        NULL,                                           \
        0,                                              \
        UNGROUP format(x));                             \
    if (actual_value_size > 0) {                        \
        if (actual_value_size < ATTEST_VALUE_BUF) {     \
            (void)snprintf(failure_info.actual_value,   \
                ATTEST_VALUE_BUF,                       \
                UNGROUP format(x));                     \
        } else {                                        \
            (void)sprintf(failure_info.actual_value,    \
                "(truncated)");                         \
        }                                               \
    }                                                   \
    int expected_label_size = snprintf(                 \
        NULL,                                           \
        0,                                              \
        "%s",                                           \
        #y);                                            \
    if (expected_label_size > 0) {                      \
        if (expected_label_size < ATTEST_VALUE_BUF) {   \
            (void)snprintf(failure_info.expected_label, \
                ATTEST_VALUE_BUF,                       \
                "%s",                                   \
                #y);                                    \
        } else {                                        \
            (void)sprintf(failure_info.expected_label,  \
                "(truncated)");                         \
        }                                               \
    }                                                   \
    int expected_value_size = snprintf(                 \
        NULL,                                           \
        0,                                              \
        UNGROUP format(y));                             \
    if (expected_value_size > 0) {                      \
        if (actual_value_size < ATTEST_VALUE_BUF) {     \
            (void)snprintf(failure_info.expected_value, \
                ATTEST_VALUE_BUF,                       \
                UNGROUP format(y));                     \
        } else {                                        \
            (void)sprintf(failure_info.expected_value,  \
                "(truncated)");                         \
        }                                               \
    }

#define EXPECT(...)                                          \
    ATTEST_EXPECT(                                           \
        PICK_ONE_FOR_CONDITION(__VA_ARGS__),                 \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__),               \
        COLLECT_ONE_VERIFICATION_TOKEN(EXPECT, __VA_ARGS__), \
        SAVE_ONE_VALUE(("%d", ), "Condition must be TRUE", __VA_ARGS__))

#define EXPECT_FALSE(...)                                          \
    ATTEST_EXPECT(                                                 \
        !(PICK_ONE_FOR_CONDITION(__VA_ARGS__)),                    \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__),                     \
        COLLECT_ONE_VERIFICATION_TOKEN(EXPECT_FALSE, __VA_ARGS__), \
        SAVE_ONE_VALUE(("%d", ), "Condition must be FALSE", __VA_ARGS__))

#define EXPECT_EQ(...)                                           \
    ATTEST_EXPECT(                                               \
        BUILD_RELATION(==, __VA_ARGS__),                         \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                  \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_EQ, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define EXPECT_NEQ(...)                                           \
    ATTEST_EXPECT(                                                \
        BUILD_RELATION(!=, __VA_ARGS__),                          \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                   \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_NEQ, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define EXPECT_GT(...)                                           \
    ATTEST_EXPECT(                                               \
        BUILD_RELATION(>, __VA_ARGS__),                          \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                  \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_GT, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define EXPECT_GTE(...)                                           \
    ATTEST_EXPECT(                                                \
        BUILD_RELATION(>=, __VA_ARGS__),                          \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                   \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_GTE, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define EXPECT_LT(...)                                           \
    ATTEST_EXPECT(                                               \
        BUILD_RELATION(<, __VA_ARGS__),                          \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                  \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_LT, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define EXPECT_LTE(...)                                           \
    ATTEST_EXPECT(                                                \
        BUILD_RELATION(<=, __VA_ARGS__),                          \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                   \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_LTE, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%lld", (long long int)), __VA_ARGS__))

#define COMPARE_STRINGS(a, b) strcmp(a, b) == 0

#define EXPECT_SAME_STRING(...)                                           \
    ATTEST_EXPECT(                                                        \
        COMPARE_STRINGS(__VA_ARGS__),                                     \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                           \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_SAME_STRING, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%s", ), __VA_ARGS__))

#define EXPECT_DIFF_STRING(...)                                           \
    ATTEST_EXPECT(                                                        \
        !(COMPARE_STRINGS(__VA_ARGS__)),                                  \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                           \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_DIFF_STRING, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%s", ), __VA_ARGS__))

#define EXPECT_SAME_CHAR(...)                                           \
    ATTEST_EXPECT(                                                      \
        BUILD_RELATION(==, __VA_ARGS__),                                \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                         \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_SAME_CHAR, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%c", ), __VA_ARGS__))

#define EXPECT_DIFF_CHAR(...)                                           \
    ATTEST_EXPECT(                                                      \
        !(BUILD_RELATION(==, __VA_ARGS__)),                             \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                         \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_DIFF_CHAR, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%c", ), __VA_ARGS__))

#define IS_NULL(ptr, ...) ptr == NULL

#define EXPECT_NULL(...)                                          \
    ATTEST_EXPECT(                                                \
        IS_NULL(__VA_ARGS__),                                     \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__),                    \
        COLLECT_ONE_VERIFICATION_TOKEN(EXPECT_NULL, __VA_ARGS__), \
        SAVE_ONE_VALUE(("%p", (void*)), "Pointer must be NULL", __VA_ARGS__))

#define EXPECT_NOT_NULL(...)                                          \
    ATTEST_EXPECT(                                                    \
        !(IS_NULL(__VA_ARGS__)),                                      \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__),                        \
        COLLECT_ONE_VERIFICATION_TOKEN(EXPECT_NOT_NULL, __VA_ARGS__), \
        SAVE_ONE_VALUE(("%p", (void*)), "Pointer must not be NULL", __VA_ARGS__))

#define EXPECT_SAME_PTR(...)                                           \
    ATTEST_EXPECT(                                                     \
        BUILD_RELATION(==, __VA_ARGS__),                               \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                        \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_SAME_PTR, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%p", (void*)), __VA_ARGS__))

#define EXPECT_DIFF_PTR(...)                                           \
    ATTEST_EXPECT(                                                     \
        !(BUILD_RELATION(==, __VA_ARGS__)),                            \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                        \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_DIFF_PTR, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%p", (void*)), __VA_ARGS__))

#define SAME_MEMORY(ptr_a, ptr_b, size, ...) memcmp(ptr_a, ptr_b, size) == 0

#define EXPECT_SAME_MEMORY(...)                                        \
    ATTEST_EXPECT(                                                     \
        SAME_MEMORY(__VA_ARGS__),                                      \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                        \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_SAME_MEM, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%p", (void*)), __VA_ARGS__))

#define EXPECT_DIFF_MEMORY(...)                                        \
    ATTEST_EXPECT(                                                     \
        !(SAME_MEMORY(__VA_ARGS__)),                                   \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__),                        \
        COLLECT_TWO_VERIFICATION_TOKENS(EXPECT_DIFF_MEM, __VA_ARGS__), \
        SAVE_TWO_VALUE(("%p", (void*)), __VA_ARGS__))

#define ATTEST_EXPECT(condition, save_msg, save_verification, save_values) \
    do {                                                                   \
        if (condition) {                                                   \
            report_success();                                              \
            break;                                                         \
        }                                                                  \
        FailureInfo failure_info = {                                       \
            .filename = __FILE__,                                          \
            .line = __LINE__,                                              \
            .has_msg = false,                                              \
        };                                                                 \
        save_msg;                                                          \
        save_verification;                                                 \
        save_values;                                                       \
        report_failure(failure_info);                                      \
    } while (0)

/**********************************************
 * LOWERCASE VERSIONS OF MACROS
 *********************************************/
#define test(...) TEST(__VA_ARGS__)

#define test_ctx(...) TEST_CTX(__VA_ARGS__)

#define before_all(...) BEFORE_ALL(__VA_ARGS__)

#define before_each(...) BEFORE_EACH(__VA_ARGS__)

#define after_all(...) AFTER_ALL(__VA_ARGS__)

#define after_each(...) AFTER_EACH(__VA_ARGS__)

#define param_test(...) PARAM_TEST(__VA_ARGS__)

#define param_test_ctx(...) PARAM_TEST_CTX(__VA_ARGS__)

#define expect(...) EXPECT(__VA_ARGS__)

#define expect_false(...) EXPECT_FALSE(__VA_ARGS__)

#define expect_eq(...) EXPECT_EQ(__VA_ARGS__)

#define expect_neq(...) EXPECT_NEQ(__VA_ARGS__)

#define expect_gt(...) EXPECT_GT(__VA_ARGS__)

#define expect_gte(...) EXPECT_GTE(__VA_ARGS__)

#define expect_lt(...) EXPECT_LT(__VA_ARGS__)

#define expect_lte(...) EXPECT_LTE(__VA_ARGS__)

#define expect_same_string(...) EXPECT_SAME_STRING(__VA_ARGS__)

#define expect_diff_string(...) EXPECT_DIFF_STRING(__VA_ARGS__)

#define expect_same_char(...) EXPECT_SAME_CHAR(__VA_ARGS__)

#define expect_diff_char(...) EXPECT_DIFF_CHAR(__VA_ARGS__)

#define expect_null(...) EXPECT_NULL(__VA_ARGS__)

#define expect_not_null(...) EXPECT_NOT_NULL(__VA_ARGS__)

#define expect_same_ptr(...) EXPECT_SAME_PTR(__VA_ARGS__)

#define expect_diff_ptr(...) EXPECT_DIFF_PTR(__VA_ARGS__)

#define expect_same_memory(...) EXPECT_SAME_MEMORY(__VA_ARGS__)

#define expect_diff_memory(...) EXPECT_DIFF_MEMORY(__VA_ARGS__)
