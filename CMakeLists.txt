cmake_minimum_required(VERSION 3.16)
project(ffbe3d_host CXX)

# Host build of the portable core + content layer and their tests.
# The console build is platform/3ds via the top-level Makefile.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

add_library(f3d src/core/battle.cpp src/content/pack.cpp src/content/encounter.cpp src/content/skills.cpp src/content/sprite.cpp src/content/units.cpp)
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

set(ENCOUNTER_PACK ${CMAKE_BINARY_DIR}/encounter.f3dp)
file(GLOB ENCOUNTER_FIXTURES ${CMAKE_SOURCE_DIR}/tests/content/fixtures/encounter/*.tsv)
add_custom_command(
  OUTPUT ${ENCOUNTER_PACK}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/assets/mstpack.py
          ${CMAKE_SOURCE_DIR}/tests/content/fixtures/encounter -o ${ENCOUNTER_PACK}
  DEPENDS tools/assets/mstpack.py tools/assets/schema.json ${ENCOUNTER_FIXTURES})

add_executable(test_encounter tests/content/test_encounter.cpp ${ENCOUNTER_PACK})
target_link_libraries(test_encounter f3d)
add_test(NAME encounter COMMAND test_encounter ${ENCOUNTER_PACK})

# PC-side pack inspector / content validator.
add_executable(f3d_inspect tools/inspect/main.cpp)
target_link_libraries(f3d_inspect f3d)

# Sprite reader test. The fixture is a synthetic unit built with Pillow; the
# fixture script also checks the converter's own output pixel-for-pixel.
set(SPRITE_FIXTURE ${CMAKE_BINARY_DIR}/fixture.f3ds)
add_custom_command(
  OUTPUT ${SPRITE_FIXTURE}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/content/make_sprite_fixture.py
          ${SPRITE_FIXTURE} ${CMAKE_BINARY_DIR}/sprite_fixture_src
  DEPENDS tests/content/make_sprite_fixture.py tools/assets/unitsprite.py tools/assets/cpk.py)
add_executable(test_sprite tests/content/test_sprite.cpp ${SPRITE_FIXTURE})
target_link_libraries(test_sprite f3d)
add_test(NAME sprite COMMAND test_sprite ${SPRITE_FIXTURE})

set(UNITS_PACK ${CMAKE_BINARY_DIR}/units.f3dp)
file(GLOB UNITS_FIXTURES ${CMAKE_SOURCE_DIR}/tests/content/fixtures/units/*)
add_custom_command(
  OUTPUT ${UNITS_PACK}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/assets/mstpack.py
          ${CMAKE_SOURCE_DIR}/tests/content/fixtures/units --preset units -o ${UNITS_PACK}
  DEPENDS tools/assets/mstpack.py tools/assets/schema.json ${UNITS_FIXTURES})
add_executable(test_units tests/content/test_units.cpp ${UNITS_PACK})
target_link_libraries(test_units f3d)
add_test(NAME units COMMAND test_units ${UNITS_PACK})
