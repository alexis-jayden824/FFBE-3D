cmake_minimum_required(VERSION 3.16)
project(ffbe3d_host CXX)

# Host build of the portable core + content layer and their tests.
# The console build is platform/3ds via the top-level Makefile.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

add_library(f3d src/core/battle.cpp src/content/pack.cpp)
target_include_directories(f3d PUBLIC src)

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(FIXTURE_PACK ${CMAKE_BINARY_DIR}/sample.f3dp)
add_custom_command(
  OUTPUT ${FIXTURE_PACK}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/assets/mstpack.py
          ${CMAKE_SOURCE_DIR}/tests/content/fixtures/T_SAMPLE.tsv -o ${FIXTURE_PACK}
  DEPENDS tools/assets/mstpack.py tests/content/fixtures/T_SAMPLE.tsv)

add_executable(test_core tests/core/test_core.cpp ${FIXTURE_PACK})
target_link_libraries(test_core f3d)
add_test(NAME core COMMAND test_core ${FIXTURE_PACK})
