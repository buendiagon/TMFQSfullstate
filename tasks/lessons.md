# Lessons

- Before reporting stale API cleanup as complete, search the repo for the exact symbols and rebuild the affected tests from source. Existing binaries can hide mismatches between the checked-in code and compiled artifacts.
- When migrating the project build system to CMake-only, remove wrapper Makefiles and stale subdirectory Makefiles instead of keeping parallel entry points that can drift from the real build graph.
