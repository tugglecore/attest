let subject_name = 'verify_attest.out'
let subject_under_test = "./" | path join "verify_attest.out";

def find_line [output: list<string>, needle: string] {
    $output
    | find -r $needle
    | is-not-empty
}

def is_valid_header [output: list<string>] {
    find_line $output '=+Test Summary=+'
}

def confirm_total [output: list<string>, count: number] {
    find_line $output $'Total:\s+($count)'
}

def confirm_passed [output: list<string>, count: number] {
    find_line $output $'Passed:\s+($count)'
}

def confirm_failed [output: list<string>, count: number] {
    find_line $output $'Failed:\s+($count)'
}

def confirm_skips [output: list<string>, count: number] {
    find_line $output $'Skipped:\s+($count)'
}

def confirm_no_assertions [output: list<string>, count: number] {
    find_line $output $'No assertions:\s+($count)'
}

def expect [test_name: string, checks: list<record>] {
    let passed_checks = $checks
        | all {|c| $c.condition}

    if $passed_checks {
        print $"(ansi green) ✅ ($test_name) example is accepted(ansi reset)"
    } else {
        print $"(ansi red) ❌ ($test_name) example is rejected(ansi reset)"
        $checks
            | where not condition
            | each { |c| print $c.msg }
        print ""
    }
}

def build_attest [name: string] {
    let args = [
        "-o" $subject_name
        "-I attest.h"
        $"example_($name).c"
    ];

    clang ...$args
}

def test_basics [] {
    build_attest basic

    let result = ^$subject_under_test | complete
    let stdout = $result.stdout | lines -s

    let will_pass_checks = [
        {
            condition: ($result.exit_code == 1),
            msg: $"    -> wrong exit code. Expected `1` but got `($result.exit_code)"
        },
        {
            condition: (is_valid_header $stdout),
            msg: $"    -> wrong header."
        },
        {
            condition: (confirm_total $stdout 5),
            msg: $"    -> wrong amount of total test."
        },
        {
            condition: (confirm_passed $stdout 1),
            msg: $"    -> wrong amount of passed test."
        },
        {
            condition: (confirm_skips $stdout 1),
            msg: $"    -> wrong amount of skips test."
        },
        {
            condition: (confirm_failed $stdout 2),
            msg: $"    -> wrong amount of failed test."
        },
        {
            condition: (confirm_no_assertions $stdout 1),
            msg: $"    -> wrong amount of `no assertion` tests."
        },
    ]

    expect 'basic' $will_pass_checks
}

def test_lifecycle [] {
    build_attest lifecycle

    let result = ^$subject_under_test | complete
    let stdout = $result.stdout | lines -s

    let will_pass_checks = [
        {
            condition: ($result.exit_code == 0),
            msg: $"    -> wrong exit code. Expected `1` but got `($result.exit_code)"
        },
        {
            condition: (is_valid_header $stdout),
            msg: $"    -> wrong header."
        },
        {
            condition: (confirm_total $stdout 1),
            msg: $"    -> wrong amount of total test."
        },
        {
            condition: (confirm_passed $stdout 1),
            msg: $"    -> wrong amount of passed test."
        },
        {
            condition: (confirm_skips $stdout 0),
            msg: $"    -> wrong amount of skips test."
        },
        {
            condition: (confirm_failed $stdout 0),
            msg: $"    -> wrong amount of failed test."
        },
    ]

    expect 'lifecycle' $will_pass_checks
}

def test_parameterization [] {
    build_attest parameters

    let result = ^$subject_under_test | complete
    let stdout = $result.stdout | lines -s

    let will_pass_checks = [
        {
            condition: ($result.exit_code == 1),
            msg: $"    -> wrong exit code."
        },
        {
            condition: (is_valid_header $stdout),
            msg: $"    -> wrong header."
        },
        {
            condition: (confirm_total $stdout 2),
            msg: $"    -> wrong amount of total test."
        },
        {
            condition: (confirm_passed $stdout 0),
            msg: $"    -> wrong amount of passed test."
        },
        {
            condition: (confirm_skips $stdout 0),
            msg: $"    -> wrong amount of skips test."
        },
        {
            condition: (confirm_failed $stdout 2),
            msg: $"    -> wrong amount of failed test."
        },
    ]

    expect 'parameters' $will_pass_checks
}

def test_expectations [] {
    mut valid_expects = true

    cd test_artifacts

    # pass EXPECT
    '#include "attest.h"
        TEST(foo) { EXPECT(1, "rings a bell"); }
    ' | save pass_expect_test.c
    clang -o pass_expect -I../ pass_expect_test.c
    let program = ^'./pass_expect' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    # fail EXPECT
    '#include "attest.h"
        TEST(foo) { EXPECT(1 + 1 == 0, "Noo"); }
    ' | save fail_expect_test.c
    clang -o fail_expect -I../ fail_expect_test.c
    let program = ^'./fail_expect' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 1)
    let valid_msg = $program.stdout | find -r Noo | is-not-empty
    $valid_expects = ($valid_expects and $valid_msg)

    # pass EXPECT_FALSE
    '#include "attest.h"
        TEST(foo) { EXPECT_FALSE(1 + 1 == 0, "Noo"); }
    ' | save pass_expect_false_test.c
    clang -o pass_expect_false -I../ pass_expect_false_test.c
    let program = ^'./pass_expect_false' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    # fail EXPECT_FALSE
    '#include "attest.h"
        TEST(foo) { EXPECT_FALSE(1 + 1 == 2, "Yess"); }
    ' | save fail_expect_false_test.c
    clang -o fail_expect_false -I../ fail_expect_false_test.c
    let program = ^'./fail_expect_false' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 1)
    let valid_msg = $program.stdout | find -r Yess | is-not-empty
    $valid_expects = ($valid_expects and $valid_msg)

    # pass EXPECT_EQ
    '#include "attest.h"
        TEST(foo) { EXPECT_EQ(1 + 1, 2, "Yess"); }
    ' | save pass_expect_eq_test.c
    clang -o pass_expect_eq -I../ pass_expect_eq_test.c
    let program = ^'./pass_expect_eq' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    # fail EXPECT_EQ
    '#include "attest.h"
        TEST(foo) { EXPECT_EQ(1,  2, "Noo"); }
    ' | save fail_expect_eq_test.c
    clang -o fail_expect_eq -I../ fail_expect_eq_test.c
    let program = ^'./fail_expect_eq' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 1)
    let valid_msg = $program.stdout | find -r Noo | is-not-empty
    $valid_expects = ($valid_expects and $valid_msg)

    # pass expect_null
    '#include "attest.h"
        test(foo) { expect_not_null(malloc(sizeof(int))); }
    ' | save pass_expect_null_test.c
    clang -o pass_expect_null -I../ pass_expect_null_test.c
    let program = ^'./pass_expect_null' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    # pass expect_same_string
    '#include "attest.h"
        test(foo) { expect_same_string("abc", "abc"); }
    ' | save pass_expect_same_string_test.c
    clang -o pass_expect_same_string -I../ pass_expect_same_string_test.c
    let program = ^'./pass_expect_same_string' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    # pass expect_gte
    '#include "attest.h"
        test(foo) { expect_gte(10, 7); }
    ' | save pass_expect_gte_test.c
    clang -o pass_expect_gte -I../ pass_expect_gte_test.c
    let program = ^'./pass_expect_gte' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 0)

    if $valid_expects {
        print $"(ansi green) ✅ expectations are accepted(ansi reset)"
    } else {
        print $"(ansi red) ❌ expecations are rejected(ansi reset)"
    }
}

def test_configuration [] {
    mut valid_expects = true

    cd test_artifacts

    # fail EXPECT
    '#include "attest.h"
        TEST(foo, .attempts = 3) { EXPECT(1 + 1 == 0); }
    ' | save attempt_test.c
    clang -o attempt_test -I../ attempt_test.c
    let program = ^'./attempt_test' | complete
    $valid_expects = ($valid_expects and $program.exit_code == 1)
    let valid_msg = $program.stdout | find -r 'Test attempt: 3' | is-not-empty
    $valid_expects = ($valid_expects and $valid_msg)

    if $valid_expects {
        print $"(ansi green) ✅ configuration are accepted(ansi reset)"
    } else {
        print $"(ansi red) ❌ configuration are rejected(ansi reset)"
    }
}

def main [] {
    print $'(ansi lp)===================================(ansi reset)'
    print $'(ansi lp)     Attest Acceptance Summary     (ansi reset)'
    print $'(ansi lp)===================================(ansi reset)'

    print $"(ansi lp)\nTests:(ansi reset)"

    mkdir test_artifacts

    try {
        test_basics
        test_lifecycle
        test_parameterization
        test_expectations
        test_configuration
    }

    rm -rf test_artifacts
    rm $subject_under_test
}
