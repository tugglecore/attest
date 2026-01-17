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

// Max buffer size to represent values
#ifndef ATTEST_VALUE_BUF
#define ATTEST_VALUE_BUF 128
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
    void* case_data;
    void* local;
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
    char* group_title;
    char* test_title;
    bool skip;
    void* data;
    int attempts;
    void (*param_init)(void);
    void (*before)(TestContext*);
    void (*before_all_cases)(ParamContext*);
    void (*before_each_case)(ParamContext*);
    void (*after_all_cases)(ParamContext*);
    void (*after_each_case)(ParamContext*);
    void (*contextual_test)(TestContext*);
    void (*simple_test)(void);
    void (*param_test)(struct TestConfig);
    void (*after)(TestContext*);
    struct TestConfig* next;
    bool disabled;
    void* values_ptr;
    size_t stride;
    int count;
    int attempt_count;
    int param_index;
    char* tags[10];
    void (*param_test_runner)(void);
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
    char reason[ATTEST_VALUE_BUF];
    bool has_display;
    char values_display[ATTEST_VALUE_BUF];
    bool has_msg;
    char msg[ATTEST_VALUE_BUF];
} FailureInfo;

typedef struct
{
    void* shared;
} GlobalContext;

typedef struct
{
    void* global_shared_data;
} AttestContext;

/**************************
 * GLOBALS
 *************************/
int total_tests = 0;
int pass_count = 0;
int fail_count = 0;
int skip_count = 0;
int empty_count = 0;

char* failed_test_titles[ATTEST_MAX_TESTS];
char* failed_group_titles[ATTEST_MAX_TESTS];

char* empty_test_titles[ATTEST_MAX_TESTS];
char* empty_group_titles[ATTEST_MAX_TESTS];

Status parameterize_instance_results[ATTEST_MAX_TESTS];
static int test_instance_count = 0;
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

static ParamContext global_param_context = {
    .shared = NULL
};

/**************************
 * PROCEDURES
 *************************/

bool is_duplicate_title(char* subject)
{
    for (int i = 0; i < fail_count; i++) {
        if (strcmp(subject, failed_test_titles[i]) == 0) {
            return true;
        }
    }

    return false;
}

void attester(TestConfig cfg)
{
    attest_internal_current_test = &cfg;
    bool is_param_test = cfg.param_test != NULL;

    if (cfg.attempts < 0) {
        fprintf(stderr, "%s[ERROR] `attempts` need to be positive.%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    if (cfg.test_title == NULL) {
        fprintf(stderr, "%s[ERROR] Test case missing title%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    if (is_duplicate_title(cfg.test_title) && !is_param_test) {
        // NOLINTNEXTLINE
        fprintf(stderr, "%s[ERROR] Duplicate Test case title%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    if (cfg.disabled) {
        return;
    }

    if (cfg.skip) {
        skip_count += 1;
        total_tests++;
        return;
    }

    int max_attempts = cfg.attempts ? cfg.attempts : 1;

    if (cfg.attempts > 0) {
        printf(
            "%s[RUN]%s %s%s%s\n",
            MAGENTA, NORMAL, BOLD_WHITE,
            attest_internal_current_test->test_title,
            NORMAL);
    }

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
            total_tests++;
        } else if (cfg.simple_test) {
            cfg.simple_test();
            total_tests++;
        } else if (cfg.param_test) {
            printf(
                "%s  [INSTANCE %d] %s",
                GRAY,
                attest_internal_current_test->param_index + 1,
                NORMAL);

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

        if (cfg.status == PASSED || cfg.status == MISSING_EXPECTATION) {
            break;
        }
    }

    switch (cfg.status) {
    case PASSED:
        if (!cfg.param_test) {
            pass_count++;
        }
        break;
    case FAILED:
        if (!cfg.param_test) {
            if (fail_count >= ATTEST_MAX_TESTS) {
                // NOLINTNEXTLINE
                fprintf(stderr,
                    "[ERROR] Reached max allowed tests. Define MACRO "
                    "ATTEST_MAX_TESTS to higher limit");
                exit(1); // NOLINT
            }
            failed_test_titles[fail_count] = cfg.test_title;
            failed_group_titles[fail_count] = cfg.group_title;
            fail_count++;
        }
        break;
    case MISSING_EXPECTATION:
        if (cfg.param_test) {
            parameterize_instance_results[cfg.param_index] = MISSING_EXPECTATION;
            printf(
                "%sMissing Assertion%s\n",
                CYAN,
                NORMAL);
        } else {
            printf(
                "%s[Missing Assertion]%s %s%s%s\n",
                MAGENTA,
                NORMAL,
                BOLD_WHITE,
                attest_internal_current_test->test_title,
                NORMAL);
            printf("%s Location:%s %s%s:%d%s\n\n",
                CYAN, NORMAL, GRAY, cfg.filename,
                cfg.line,
                NORMAL);
            empty_count++;
        }
        break;
    default:
        printf("ATTEST ERROR. print debug logs and file issue");
    }
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
    } else if (current_test->param_test) {
        parameterize_instance_results[current_test->param_index] = PASSED;

        printf(
            "%s Passed%s\n",
            GREEN,
            NORMAL);
    }
}

void report_failed_expectation(FailureInfo failure_info)
{
    attest_internal_current_test->status = FAILED;

    printf(
        "%s[Failed] %s%s%s\n",
        RED,
        BOLD_WHITE,
        attest_internal_current_test->test_title, NORMAL);

    printf("%s Check: %s%s\n",
        CYAN, NORMAL, failure_info.reason);

    if (failure_info.has_display) {
        printf("%s Values:%s %s\n",
            CYAN, NORMAL, failure_info.values_display);
    }

    if (failure_info.has_msg) {
        printf("%s Message: %s%s\n",
            CYAN, NORMAL, failure_info.msg);
    }

    printf("%s Location:%s %s%s:%d%s\n\n",
        CYAN, NORMAL, GRAY, failure_info.filename,
        failure_info.line, NORMAL);
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
        parameterize_instance_results[attest_internal_current_test->param_index] = FAILED;
    }

    if (attest_internal_current_test->param_test) {
        printf(
            "%s Failed \n%s",
            RED,
            NORMAL);
    } else if (attest_internal_current_test->attempts == 0) {
        report_failed_expectation(failure_info);
    } else {
        report_failed_attempt(failure_info);
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

static TestConfig* attest_registry_head = NULL;

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

bool has_status(Status status)
{
    for (int i = 0; i < test_instance_count; i++) {
        if (parameterize_instance_results[i] == status) {
            return true;
        }
    }
    return false;
}

bool has_failed_tests()
{
    return has_status(FAILED);
}

bool has_missing_tests()
{
    return has_status(MISSING_EXPECTATION);
}

void parameterize_test(TestConfig* test_config)
{
    test_config->param_init();
    if (test_instance_count == 0) {
        fprintf(stderr, "%s[ATTEST ERROR] Pass parenthesis enclosed values when using `PARAM_TEST` or `PARAM_TEST_CTX`.%s\n", RED, NORMAL);
        exit(1); // NOLINT
    }

    printf(
        "%s[PARAMETERIZE]%s %s%s%s\n",
        MAGENTA, NORMAL, BOLD_WHITE,
        test_config->test_title,
        NORMAL);

    global_param_context.shared = attest_context.global_shared_data;

    if (parameterize_before_all_cases != NULL) {
        parameterize_before_all_cases(&global_param_context);
    }

    test_config->param_test_runner();

    total_tests++;

    // TODO: Add count of how many failed/empty instances they are
    // Currently parameter tests show which instance has failed
    // and that's it. We need to show total instances and pass/fail
    // count for entire parameterize test.
    if (has_failed_tests()) {
        failed_test_titles[fail_count] = test_config->test_title;
        fail_count++;

        printf(
            "%s[FAILED]%s %s%s%s\n",
            RED, NORMAL, BOLD_WHITE,
            test_config->test_title,
            NORMAL);
    } else if (has_missing_tests()) {
        empty_test_titles[empty_count] = test_config->test_title;
        empty_count++;
        printf(
            "%s[MISSING ASSERTION] %s%s\n",
            CYAN,
            test_config->test_title,
            NORMAL);
    } else {
        pass_count++;
        printf(
            "%s[PASSED] %s%s\n",
            MAGENTA,
            test_config->test_title,
            NORMAL);
    }

    if (parameterize_after_all_cases != NULL) {
        parameterize_after_all_cases(&global_param_context);
    }

    global_param_context.shared = NULL;
    global_param_context.set = NULL;
    global_param_context.local = NULL;

    parameterize_before_all_cases = NULL;
    parameterize_after_all_cases = NULL;
    test_instance_count = 0;
}

#ifndef ATTEST_NO_MAIN
int main(void)
{
    GlobalContext global_context = { .shared = NULL };

    if (attest_before_all_handler) {
        attest_before_all_handler(&global_context);

        if (global_context.shared != NULL) {
            attest_context.global_shared_data = global_context.shared;
        }
    }

    TestConfig* t = attest_registry_head;

    while (t) {
        if (t->param_test_runner) {
            parameterize_test(t);
        } else {
            attester(*t);
        }

        t = t->next;
    }

    if (attest_after_all_handler) {
        attest_after_all_handler(&global_context);
    }

    report_summary();

    return 0;
}
#endif

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

#define PARAM_TEST(name, param_type, param_var, values_group, ...)      \
    void name##_impl(param_type param_var);                             \
    void name##_impl_wrapper(TestConfig cfg);                           \
    static const param_type name##_data[] = {                           \
        STRIP_PARENS(values_group)                                      \
    };                                                                  \
                                                                        \
    void name##_init(void)                                              \
    {                                                                   \
        TestConfig cfg = { __VA_ARGS__ };                               \
        test_instance_count = sizeof(name##_data) / sizeof(param_type); \
        parameterize_before_all_cases = cfg.before_all_cases;           \
        parameterize_after_all_cases = cfg.after_all_cases;             \
    }                                                                   \
    void name##_runner(void)                                            \
    {                                                                   \
        for (int i = 0; i < test_instance_count; i++) {                 \
            TestConfig param_cfg = {                                    \
                .filename = __FILE__,                                   \
                .line = __LINE__,                                       \
                .test_title = #name,                                    \
                .param_test = name##_impl_wrapper,                      \
                .param_index = i,                                       \
                __VA_ARGS__                                             \
            };                                                          \
            global_param_context.case_data = (void*)&name##_data[i];    \
            attester(param_cfg);                                        \
        }                                                               \
    }                                                                   \
    static void __attribute__((constructor))                            \
    register_##name##_runner(void)                                      \
    {                                                                   \
        static TestConfig test_config = {                               \
            .test_title = #name,                                        \
            .param_test_runner = name##_runner,                         \
            .param_init = name##_init,                                  \
        };                                                              \
        attest_update_registry(&test_config);                           \
    }                                                                   \
    void name##_impl_wrapper(TestConfig cfg)                            \
    {                                                                   \
        name##_impl(name##_data[cfg.param_index]);                      \
    }                                                                   \
    void name##_impl(param_type param_var)

#define PARAM_TEST_CTX(name,                                                \
    context, param_type, param_var, values_group, ...)                      \
    void name##_impl(ParamContext* context, param_type param_var);          \
    void name##_impl_wrapper(TestConfig cfg);                               \
    static const param_type name##_data[] = {                               \
        STRIP_PARENS(values_group)                                          \
    };                                                                      \
                                                                            \
    void name##_init(void)                                                  \
    {                                                                       \
        TestConfig cfg = { __VA_ARGS__ };                                   \
        test_instance_count = sizeof(name##_data) / sizeof(param_type);     \
        parameterize_before_all_cases = cfg.before_all_cases;               \
        parameterize_after_all_cases = cfg.after_all_cases;                 \
    }                                                                       \
    void name##_runner(void)                                                \
    {                                                                       \
        for (int i = 0; i < test_instance_count; i++) {                     \
            TestConfig param_cfg = {                                        \
                .filename = __FILE__,                                       \
                .line = __LINE__,                                           \
                .test_title = #name,                                        \
                .param_test = name##_impl_wrapper,                          \
                .param_index = i,                                           \
                __VA_ARGS__                                                 \
            };                                                              \
            global_param_context.case_data = (void*)&name##_data[i];        \
            attester(param_cfg);                                            \
        }                                                                   \
    }                                                                       \
    static void __attribute__((constructor)) register_##name##_runner(void) \
    {                                                                       \
        static TestConfig test_config = {                                   \
            .test_title = #name,                                            \
            .param_test_runner = name##_runner,                             \
            .param_init = name##_init,                                      \
        };                                                                  \
        attest_update_registry(&test_config);                               \
    }                                                                       \
    void name##_impl_wrapper(TestConfig cfg)                                \
    {                                                                       \
        name##_impl(&global_param_context, name##_data[cfg.param_index]);   \
    }                                                                       \
    void name##_impl(ParamContext* context, param_type param_var)

/**************************
 * EXPECTATIONS
 *************************/
// TODO: We are voiding the return values of `sn?printf` to silence
// the warning from clang-tidy check `cert-err3-c`. Decide if it is
// worth the increase in macro compelxity to properly handle result.
#define PICK_ONE(x, ...) x

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

#define SAVE_MESSAGE(...)                                                    \
    size_t msg_size = snprintf(NULL, 0, __VA_ARGS__);                        \
    if (msg_size > 0) {                                                      \
        failure_info.has_msg = true;                                         \
        if (msg_size < ATTEST_VALUE_BUF) {                                   \
            (void)snprintf(failure_info.msg, ATTEST_VALUE_BUF, __VA_ARGS__); \
        } else {                                                             \
            (void)sprintf(failure_info.msg, "(truncated)");                  \
        }                                                                    \
    }

#define TAKE_ONE(x, ...) __VA_ARGS__
#define TAKE_TWO(x, y, ...) __VA_ARGS__

#define BUILD_RELATION_REASON(relation, x, y, ...) \
    "[ %s %s %s ]", #x, #relation, #y

#define BUILD_CONDITION_REASON(type, x, ...) \
    "[ %s ] should be %s", #x, #type

#define BUILD_RELATION(operator, x, y, ...) x operator y

#define DISPLAY_VALUES(...)                                            \
    failure_info.has_display = true;                                   \
    size_t display_size = snprintf(NULL, 0, __VA_ARGS__);              \
    if (display_size > 0) {                                            \
        if (display_size < ATTEST_VALUE_BUF) {                         \
            (void)snprintf(failure_info.values_display,                \
                ATTEST_VALUE_BUF,                                      \
                __VA_ARGS__);                                          \
        } else {                                                       \
            (void)sprintf(failure_info.values_display, "(truncated)"); \
        }                                                              \
    }

#define IGNORE_DISPLAY

#define DISPLAY_RELATION(op, x, y, ...) \
    DISPLAY_VALUES("%lld %s %lld", (long long int)(x), #op, (long long int)(y))

#define EXPECT(...)                            \
    ATTEST_EXPECT_ENGINE(                      \
        PICK_ONE(__VA_ARGS__),                 \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__), \
        IGNORE_DISPLAY,                        \
        BUILD_CONDITION_REASON(true, __VA_ARGS__))

#define EXPECT_FALSE(...)                      \
    ATTEST_EXPECT_ENGINE(                      \
        !(PICK_ONE(__VA_ARGS__)),              \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__), \
        IGNORE_DISPLAY,                        \
        BUILD_CONDITION_REASON(false, __VA_ARGS__))

#define EXPECT_EQ(...)                          \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(==, __VA_ARGS__),        \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(==, __VA_ARGS__),      \
        BUILD_RELATION_REASON(==, __VA_ARGS__))

#define EXPECT_NEQ(...)                         \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(!=, __VA_ARGS__),        \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(!=, __VA_ARGS__),      \
        BUILD_RELATION_REASON(!=, __VA_ARGS__))

#define EXPECT_GT(...)                          \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(>, __VA_ARGS__),         \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(>, __VA_ARGS__),       \
        BUILD_RELATION_REASON(<=, __VA_ARGS__))

#define EXPECT_GTE(...)                         \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(>=, __VA_ARGS__),        \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(>=, __VA_ARGS__),      \
        BUILD_RELATION_REASON(<, __VA_ARGS__))

#define EXPECT_LT(...)                          \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(<, __VA_ARGS__),         \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(<, __VA_ARGS__),       \
        BUILD_RELATION_REASON(>=, __VA_ARGS__))

#define EXPECT_LTE(...)                         \
    ATTEST_EXPECT_ENGINE(                       \
        BUILD_RELATION(<=, __VA_ARGS__),        \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_RELATION(<=, __VA_ARGS__),      \
        BUILD_RELATION_REASON(>, __VA_ARGS__))

#define COMPARE_STRINGS(a, b) strcmp(a, b) == 0

#define DISPLAY_STRING(x, y, ...) \
    DISPLAY_VALUES("LHS=\"%s\", RHS=\"%s\"", x, y)

#define EXPECT_SAME_STRING(...)                 \
    ATTEST_EXPECT_ENGINE(                       \
        COMPARE_STRINGS(__VA_ARGS__),           \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_STRING(__VA_ARGS__),            \
        BUILD_RELATION_REASON(==, __VA_ARGS__))

#define EXPECT_DIFF_STRING(...)                 \
    ATTEST_EXPECT_ENGINE(                       \
        !(COMPARE_STRINGS(__VA_ARGS__)),        \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_STRING(__VA_ARGS__),            \
        BUILD_RELATION_REASON(==, __VA_ARGS__))

#define IS_NULL(ptr, ...) ptr == NULL

#define BUILD_PTR_REASON(status, ptr, ...) \
    "Pointer [ %s ] should %s", #ptr, #status

#define DISPLAY_STRING(x, y, ...) \
    DISPLAY_VALUES("LHS=\"%s\", RHS=\"%s\"", x, y)

#define EXPECT_NULL(...)                       \
    ATTEST_EXPECT_ENGINE(                      \
        IS_NULL(__VA_ARGS__),                  \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__), \
        IGNORE_DISPLAY,                        \
        BUILD_PTR_REASON(be NULL, __VA_ARGS__))

#define EXPECT_NOT_NULL(...)                   \
    ATTEST_EXPECT_ENGINE(                      \
        !(IS_NULL(__VA_ARGS__)),               \
        MSG_DISPATCH_FOR_ONE_ARG(__VA_ARGS__), \
        IGNORE_DISPLAY,                        \
        BUILD_PTR_REASON(not be NULL, __VA_ARGS__))

#define SAME_PTR(ptr_a, ptr_b, ...) ptr_a == ptr_b

#define EXPECT_SAME_PTR(...)                    \
    ATTEST_EXPECT_ENGINE(                       \
        SAME_PTR(__VA_ARGS__),                  \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        IGNORE_DISPLAY,                         \
        BUILD_RELATION_REASON(==, __VA_ARGS__))

#define EXPECT_DIFF_PTR(...)                    \
    ATTEST_EXPECT_ENGINE(                       \
        !(SAME_PTR(__VA_ARGS__)),               \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        IGNORE_DISPLAY,                         \
        BUILD_RELATION_REASON(!=, __VA_ARGS__))

#define SAME_MEMORY(ptr_a, ptr_b, size, ...) memcmp(ptr_a, ptr_b, size) == 0

#define EXPECT_SAME_MEMORY(...)                 \
    ATTEST_EXPECT_ENGINE(                       \
        SAME_MEMORY(__VA_ARGS__),               \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_BOOLEAN(__VA_ARGS__),           \
        BUILD_RELATION_REASON(==, __VA_ARGS__))

#define EXPECT_DIFF_MEMORY(...)                 \
    ATTEST_EXPECT_ENGINE(                       \
        !(SAME_MEMORY(__VA_ARGS__)),            \
        MSG_DISPATCH_FOR_TWO_ARGS(__VA_ARGS__), \
        DISPLAY_BOOLEAN(__VA_ARGS__),           \
        BUILD_RELATION_REASON(!=, __VA_ARGS__))

#define ATTEST_EXPECT_ENGINE(condition, value_display, save_msg, ...) \
    do {                                                              \
        if (condition) {                                              \
            report_success();                                         \
            break;                                                    \
        }                                                             \
        FailureInfo failure_info = {                                  \
            .filename = __FILE__,                                     \
            .line = __LINE__,                                         \
            .has_msg = false,                                         \
            .has_display = false,                                     \
        };                                                            \
        save_msg;                                                     \
        value_display;                                                \
        size_t reason_size = snprintf(NULL, 0, __VA_ARGS__);          \
        if (reason_size < ATTEST_VALUE_BUF) {                         \
            (void)snprintf(failure_info.reason,                       \
                ATTEST_VALUE_BUF, __VA_ARGS__);                       \
        } else {                                                      \
            (void)sprintf(failure_info.reason,                        \
                "Expression is false: (truncated)");                  \
        }                                                             \
        report_failure(failure_info);                                 \
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

#define expect_null(...) EXPECT_NULL(__VA_ARGS__)

#define expect_not_null(...) EXPECT_NOT_NULL(__VA_ARGS__)

#define expect_same_ptr(...) EXPECT_SAME_PTR(__VA_ARGS__)

#define expect_diff_ptr(...) EXPECT_DIFF_PTR(__VA_ARGS__)

#define expect_same_memory(...) EXPECT_SAME_MEMORY(__VA_ARGS__)

#define expect_diff_memory(...) EXPECT_DIFF_MEMORY(__VA_ARGS__)
