# Lessons

- Before reporting stale API cleanup as complete, search the repo for the exact symbols and rebuild the affected tests from source. Existing binaries can hide mismatches between the checked-in code and compiled artifacts.
- When migrating the project build system to CMake-only, remove wrapper Makefiles and stale subdirectory Makefiles instead of keeping parallel entry points that can drift from the real build graph.
- When proposing backend architecture changes, preserve the storage model the user asked for. Do not silently turn a compressed backend into a mostly-dense resident working set unless that tradeoff was explicitly requested.
- Before concluding a local binary cannot be executed, check the repo's documented runtime environment setup files as well as the README. Project-local shell bootstrap scripts may be required even when the general docs imply RPATH coverage.
- For very fast binaries, do not attach profilers after launch unless there is an explicit handshake. Use a stop-before-exec or equivalent mechanism so the profiler cannot lose the process before the first sample.
- Before retrying a broken experiment sweep, clear the partial output tree the user wants replaced so stale artifacts do not mask whether the new runner is behaving correctly.
