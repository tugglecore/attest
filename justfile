ext := if os_family() == "windows" { "exe" } else { "out" }

watchexec_options := if os_family() == "windows" {
    "--shell=cmd -w 'attest.h' -w 'example_basic.c' -w 'example_lifecycle.c' -w 'example_parameters.c'"
} else {
    "-i '*.out' -i test_artifacts -i 'test_artifacts/**'"
}

dev target:
    watchexec {{watchexec_options}} 'clear && just run {{target}}'

@run command:
    clang -Wall -Wextra -std=c99 -I ./ -I demo_program example_{{command}}.c
    -./a.{{ext}}
    rm ./a.{{ext}}

test:
    nu regression_tests.nu

watch-test:
    watchexec {{watchexec_options}} 'clear && just test'

format:
    clang-format --style=file -i --sort-includes *.[c,h]

cppcheck:
    cppcheck --error-exitcode=1 --check-level=exhaustive --language=c --enable=warning,style,performance,information attest.h

clang-tidy:
    run-clang-tidy -p=build src/*.[c,h]

vale:
    vale $(rg --files | rg '.md[x]?$')

check: 
    watchexec 'clear && just vale cppcheck clang-tidy'

infer:
    infer \
        --compilation-database build/compile_commands.json \
        --skip-analysis-in-path subprojects \
        --headers \
        --bufferoverrun \
        --loop-hoisting

valgrind *command:
    valgrind \
        --error-exitcode=1 \
        --track-fds=bad \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        build/src/rudolph {{command}}
    valgrind \
        --tool=drd \
        --error-exitcode=1 \
        --track-fds=bad \
        --trace-fork-join=yes \
        --trace-mutex=yes \
        build/src/rudolph {{command}}
    valgrind \
        --tool=helgrind \
        --error-exitcode=1 \
        --track-fds=bad \
        --free-is-write=yes \
        build/src/rudolph {{command}}

