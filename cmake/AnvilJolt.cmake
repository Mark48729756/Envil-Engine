# Fetches Jolt Physics when ANVIL_WITH_JOLT=ON. Included by the full build and the physics-only build.
# Set FETCHCONTENT_SOURCE_DIR_JOLTPHYSICS=<path> to use a local checkout instead of downloading.
if(ANVIL_WITH_JOLT AND NOT TARGET Jolt)
  include(FetchContent)
  set(JPH_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(JPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(JPH_BUILD_VIEWER OFF CACHE BOOL "" FORCE)
  set(JPH_BUILD_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
  set(JPH_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
  set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
  set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
  set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
  # Jolt keeps its CMakeLists.txt in the Build/ subfolder (M32 fix: SOURCE_SUBDIR was missing before).
  FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0 GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build)
  FetchContent_MakeAvailable(JoltPhysics)
endif()
