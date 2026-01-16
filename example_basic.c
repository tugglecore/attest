#include "attest.h"

// Here is the most simple form of a test in Attest. The first
// parameter to any Attest macro is always the name of the tests
// In the body you can use any Attest expectation macro.
TEST(can_add_two_numbers)
{
    int expected = 3;
    int actual = 3;
    EXPECT_EQ(actual, expected);
}

// Now, before we go any further lets talk about the one thing that
// is always required for any test in Attest: assertions. Every test
// needs atleast one assertion. A test without a single assertion
// will be reported as 'No assertions'.
TEST(a_test_without_assertions_is_a_problem)
{
}

// Okay, anytime an expectation failed it will be reported as a
// failed test. You can find in the output the line at which the
// expectation failed along with the expressions in the expectation.
TEST(intentionally_failed_expectation)
{
    EXPECT_EQ(3, 88);
}

// Now, lets introduce a new category of features: configurations.
// Configurations allow you to change the behavior of a test. The
// first one we will talk about is skipping a test. When skipping
// a test the logic in the test function along with the any
// associated lifecycle methods are not executed. The skip function
// is counted in the Attest's summary.
TEST(skipping_a_test, .skip = true)
{
    EXPECT_EQ(0, 88);
}

// Onto a similar concept to skipping a test: disabling a test. The
// only difference between a disabled test and a skip test is that
// the skip test is reported in the summary while the disabled test is
// not reported in the summary. And to be complete the test and the
// lifecycle methods for a disabled test is not executed.
TEST(disabling_a_test, .disabled = true)
{
    EXPECT_EQ(9, 88);
}

// Sometimes a test needs to executed multiple times and this is
// where the retries configuration comes in to play. You specify
// an amount of times the test needs to execute then the test along
// with any related lifecycle methods are execute up to the specified
// amount or until the test passes
TEST(retrying_a_test, .attempts = 3)
{
    EXPECT_EQ(4, 3);
}
