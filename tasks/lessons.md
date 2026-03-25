# Lessons

- Before reporting stale API cleanup as complete, search the repo for the exact symbols and rebuild the affected tests from source. Existing binaries can hide mismatches between the checked-in code and compiled artifacts.
- When migrating the project build system to CMake-only, remove wrapper Makefiles and stale subdirectory Makefiles instead of keeping parallel entry points that can drift from the real build graph.
- When proposing backend architecture changes, preserve the storage model the user asked for. Do not silently turn a compressed backend into a mostly-dense resident working set unless that tradeoff was explicitly requested.
