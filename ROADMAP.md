# Roadmap

 - fail fast option
 - list all tests
 - `--ascii` option to disable ut8 output like symbols
 - `--timeout=<millisecond_limit>` for all tests
 - `--always-succeed` will make the process always succeed
 - Parallel multi process testing
 - Timeouts for tests
 - Change directory and create random directory
 - allow user to redefine main function via `ATTEST_NO_MAIN`
 - register a callback to be called after all tests with a detailed test summary
 - accept CLI argument `filter=pattern` to select which tests will run 
 - Suppport compiling MSVC. Currently, compiling with MSVC fails with errors related to our usage of GCC/Clang attributes.
 - Provide log function in order provide a better test logging experince
