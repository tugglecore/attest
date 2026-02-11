ext := if os_family() == "windows" { "exe" } else { "out" }

watchexec_options := if os_family() == "windows" {
    "--shell=cmd -w 'attest.h' -w 'example_basic.c' -w 'example_lifecycle.c' -w 'example_parameters.c'"
} else {
    "-i '*.out' -i test_artifacts -i 'test_artifacts/**'"
}

dev target *args:
    watchexec {{watchexec_options}} 'clear && just run {{target}} args'

@run command *args:
    clang -Wall -Wextra -fsanitize=address -std=c99 -I ./ -I demo_program example_{{command}}.c
    -./a.{{ext}} {{args}}
    rm ./a.{{ext}}

test:
    nu regression_tests.nu

watch-test:
    watchexec {{watchexec_options}} 'clear && just test'

format:
    clang-format --style=file -i --sort-includes *.[c,h]

clang-tidy:
    run-clang-tidy -p=build src/*.[c,h]

vale:
    vale $(rg --files | rg '.md[x]?$')
