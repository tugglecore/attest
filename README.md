# Attest

Single header, C99 compatible, cross-platform testing runner.

## Features
 - **Cross-platorm:** Supports Windows, MacOS and Linux
 - **Zero dynamic allocation:** Uses only static storage
 - Parameterize Tests
 - Lifecycle functions
 - Configurable test behavior

## Basic usage
```c
#include "attest.h"

TEST(basic_math) {
    int expected = 7;
    int actual = 3 + 4;

    EXPECT_EQ(actual, expected);
}
```

## Installation
Drop the header file anywhere in your project's include directory. Then include it like so:
```c
#include "attest.h"
```

## API

**Note on naming**, all macros have lowercase versions.

### `TEST(name, [options...])`

Defines a test case. This macro automatically registers the case with the runner.

**Parameters:**
- `name`: Unique name for the test case. No spaces or quotes

**Options:**
The options may appear in any order. You pass the options as arguments prefixed with a dot. 
|Option             |Type         |Default  |Description                  |
|-------------------|-------------|---------|-----------------------------|
|`.disabled`        |`bool`       |`false`  |If true, the runner ignores the test and doesn't report |
|`.skip`            |`bool`       |`false`  |If true, the runner ignores the test but still report it |
|`.attempts`        |`int`        |`1`      |The number of times to execute the test body      |
|`.before_each`|`void(*)(TextContext*)`|`NULL`|A function that runs before the test.|
|`.after_each`|`void(*)(TextContext*)`|`NULL`|A function that runs after the test. |

**Example:**
```c
TEST(hit_api, .attempts = 10) {
    int result = handle_api_request();
    EXPECT_EQ(result, 200);
}
```



### `TestContext`

Attest passes a `TestContext` object to each lifecycle and `TEST_CTX` function.

**Fields:**
|name            |Type         |Description                               |
|----------------|-------------|------------------------------------------|
|`shared`        |`void*`      |Data intended shared amongst all tests and lifecycle functions. Value available for entire program. User cleanup data.|
|`local`         |`void*`      |Data intended for a single tests. Value set to `NULL` before each triplet of `BEFORE_EACH`, `TEST_CTX` and `AFTER_EACH`. Attest cleanup data.|

**Example:**
```c
#include "attest.h"

BEFORE_ALL(test_context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    test_context->shared = (void*)foo;
}

TEST_CTX(with_a_context, test_context)
{
    int global_num = *(int*)test_context->shared;

    EXPECT_EQ(global_num + local_num, 21);
}
```



### `TEST_CTX(name, test_context, [options...])`

Defines a test case that accepts a `Context` object.

**Parameters:**
- `name`: Unique name for the test case. No spaces or quotes
- `test_context`: Has type `TestContext` and allows sharing allocated data.

**Options:**
Accept all the options available to `TEST`.

**Example:**
```c
#include "attest.h"

BEFORE_ALL(test_context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    test_context->shared = (void*)foo;
}

TEST_CTX(with_a_context, test_context)
{
    int global_num = *(int*)test_context->shared;

    EXPECT_EQ(global_num + local_num, 21);
}
```



### `BEFORE_ALL(test_context)`

A lifecycle function that runs before all tests and lifecycle functions.

**Parameters:**
- `test_context`: Has type `TestContext` and allows sharing allocated data.

**Example:**
```c
BEFORE_ALL(test_context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    test_context->shared = (void*)foo;
}
```



### `BEFORE_EACH(test_context)`

A lifecycle function that runs before each test.

**Parameters:**
- `test_context`: Has type `TestContext` and allows sharing allocated data.

**Example:**
```c
BEFORE_EACH(test_context)
{
    int* foo = malloc(sizeof(int));
    *foo = *(int*)test_context->shared - 3;
    test_context->local = (void*)foo;
}
```



### `AFTER_EACH(test_context)`

A lifecycle function that runs after each tests.

**Parameters:**
- `test_context`: Has type `TestContext` and allows sharing allocated data.

**Example:**
```c
AFTER_EACH(test_context)
{
    free(test_context->local);
    test_context->local = NULL;
}
```



### `AFTER_ALL(test_context)`

A lifecycle function that runs after all tests and lifecycle functions.

**Parameters:**
- `test_context`: Has type `TestContext` and allows sharing allocated data.

**Example:**
```c
AFTER_ALL(test_context)
{
    free(test_context->shared);
}
```



### `PARAM_TEST(name, case_type, case_name, (values), [options...])`

**Parameters:**
- `name`: Unique name for the parameterized test. No spaces or quotes.
- `case_type`: case data type.
- `case_name`: Name of case data.
- `values`: a list of values of type `case_type` enclosed in parenthesis.

**Options:**

|Option              |Type                    |Description                 |
|--------------------|------------------------|----------------------------|
|`.before_all_cases` |`void(*)(ParamContext*)`|A test that runs before all cases.|
|`.before_each_case` |`void(*)(ParamContext*)`|A test that runs before each case.|
|`.after_all_cases`  |`void(*)(ParamContext*)`|A test that runs after all cases. |
|`.after_each_cases` |`void(*)(ParamContext*)`|A test that runs after each case.|

**Example:**
```c
#include "attest.h"

PARAM_TEST(fruit_basket,
    int,
    case_num,
    (1, 2, 3))
{
    EXPECT_EQ(case_num, 1);
}
```



### `ParamContext`

Attest passes a `ParamContext` object to each test case of a parameterized tests and each parameterized lifecycle function.

**Fields:**
|name            |Type      |Description                                   |
|----------------|----------|----------------------------------------------|
|`shared`        |`void*`   |Data intended shared amongst all tests and lifecycle functions. Value available for entire program.|
|`local`         |`void*`   |Data intended for single parameterized test instance. Value set to `NULL` before each triplet `BEFORE_EACH_CASE`, `AFTER_EACH_CASE` and a single instance of `PARAM_TEST_CASE`.|
|`set`           |`void*`   |Data intended for entire parameterized test. Should set in `.before_all_cases`. Data available for all parameterized cases.|

**Example:**
```c
#include "attest.h"

PARAM_TEST_CTX(basket_case,
    param_context,
    int,
    case_num,
    int, 1, 2, 3)
{
    int shared_num = *(int*)param_context->shared;
    EXPECT_EQ(shared_num, case_num);
}
```



### `PARAM_TEST_CTX(name, param_contest, case_type, case_name, (values), [options...])`
`

**Parameters:**
- `name`: Unique name for the parameterized test. No spaces or quotes.
- `param_context`: Name of `ParamContext`.
- `case_type`: case data type.
- `case_name`: Name of case data.
- `values`: a list of values of type `case_type` enclosed in parenthesis.

**Options:**
Accept all the options available to `PARAM_TEST`.

**Example:**
```c
#include "attest.h"

PARAM_TEST_CTX(basket_case,
    param_context,
    int,
    case_num,
    (1, 2, 3))
{
    int global_num = *(int*)context->shared;
    EXPECT_EQ(global_num, case_num);
}
```

## Expectations
Attest only provide expectations. Expectations don't stop the test, attest execute their arguments exactly once and tests can have more than one.

For each expectation, you can pass it variable amount arguments passed to it and used those arguments to create a formatted message.

**Example:**
```c
TEST(hit_api, .attempts = 10) {
    int expected_status = 200;

    int result = handle_api_request();

    EXPECT_EQ(result, expected_status, "Should return %d, but got %d", expected_status, result);
}
```

- `EXPECT_`: Records a failure but continues the test execution.

**List of Expectation:**
|Macro                     | Arguments        |Description               |
|--------------------------|------------------|------------------------- |
|EXPECT(x)                 | `bool`           |Confirm true expression.  |
|EXPECT_FALSE(x)           | `bool`           |Confirm false expression. |
|EXPECT_SAME_STRING(a, b)  | `char*`, `char*` |Confirm same strings.     |
|EXPECT_DIFF_STRING(a, b)  | `char*`, `char*` |Confirm different strings.|
|EXPECT_SAME_MEMORY(a, b)  | `void*`, `void*` |Confirm same memory.      |
|EXPECT_DIFF_MEMORY(a, b)  | `void*`, `void*` |Confirm different memory. |
|EXPECT_SAME_PTR(a, b)     | `*`, `*`         |Confirm same pointer.     |
|EXPECT_DIFF_PTR(a, b)     | `*`, `*`         |Confirm different pointer.|
|EXPECT_NULL(x)            | `*`              |Confirm `NULL` pointer.   |
|EXPECT_NOT_NULL(x)        | `*`              |Confirm not `NULL` pointer|
|EXPECT_EQ(a, b)           |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a == b`. |
|EXPECT_NEQ(a, b)          |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a != b`.  |
|EXPECT_LT(a, b)           |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a < b`.  |
|EXPECT_LTE(a, b)          |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a <= b`.   |
|EXPECT_GT(a, b)           |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a > b`.   |
|EXPECT_GTE(a, b)          |`<any integer>`, `<any integer>` |Cast each argument to a `long long int` and check `a >= b`.   |

## Runner options
Macros to change the behavior of Attest. You must define runner options before including `attest.h`.

**Options:**

|Macro              |Type         |Default  |Description                  |
|-------------------|-------------|---------|-----------------------------|
|`ATTEST_MAX_TESTS` |`int`        |`128`    |Max number of tests allowed per binary.  |
|`ATTEST_NO_COLOR`  |`bool`       |`false`  |Disables ANSI color codes in output report.|
|`ATTEST_VALUE_BUF` |`int`        |`128`    |The max size of buffer used in failure messages. |
|`ATTEST_MAX_PARAMERTERIZE_RESULTS` |`int`        |`32`    |The max amount of failures for a parameterize test. |

**Example:**
```c
#define ATTEST_NO_COLOR 1
#include "attest.h"
```

## Attest behavior

### Compatibility:
Supports Clang on Windows and GCC/Clang on all *nixes. The team built Attest with Clang on Windows MacOS and Fedora Linux.

On Windows, Attest does not compile with MSVC. Although, Attest is not compatible with MSVC currently but this is on the Roadmap and important to author. 

**Platform support:**
 - *nixes (GCC/Clang)
 - MacOS (GCC/Clang)
 - Windows (Clang)

### Undefined behavior policy such as segmentation faults
Thank you for asking. Attest don't catch or recover from segmentation faults.
If the user's code segfaults, the OS terminates the test process immediately, just like any normal C program. Attest don't intercept signals, fork processes, or attempt to continue execution after undefined behavior.

### Test execution order:

Normal Tests:
 1. `BEFORE_ALL`
 2. `BEFORE_EACH`
 3. `TEST` or `TEST_CTX`
 4. `AFTER_EACH`
 5. `AFTER_ALL`

Parameterized Tests:
 1. `BEFORE_ALL`
 2. `BEFORE_ALL_CASES`
 3. `BEFORE_EACH_CASE`
 4. `PARAM_TEST` or `PARAM_TEST_CTX`
 5. `AFTER_EACH_CASE`
 6. `AFTER_ALL_CASES`
 7. `AFTER_ALL`
