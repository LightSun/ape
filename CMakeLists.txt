cmake_minimum_required(VERSION 2.8.12)
project(ape C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -g -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC")

#file(GLOB SRC_APE ape/*.h ape/*.c)
#file(GLOB_RECURSE SRC_TESTS tests/*.h tests/*.c)

#add_library(ape
#    ${SRC_APE}
#    )
#add_executable(demo_api
#    examples/api.c
# )
#target_include_directories(ape PUBLIC ape/)
#target_include_directories(ape PUBLIC ./)
#target_include_directories(demo_api PUBLIC ape/)
#target_include_directories(demo_api PUBLIC ./)
#target_link_libraries(demo_api PUBLIC ape m)
## tests
#add_executable(unit_tests
#    ${SRC_TESTS}
#    )
#target_compile_definitions(unit_tests PUBLIC
#    APE_TESTS_MAIN
#    COLLECTIONS_DEBUG
#    APE_DEBUG
#    )
#target_link_libraries(unit_tests PUBLIC ape m)

# h7 core
file(GLOB SRC_H7_CORE h7/*.h h7/*.c)
file(GLOB SRC_H7_TESTS h7/tests/*.h h7/tests/*.c)
add_library(libh7
    ${SRC_H7_CORE}
    )
target_include_directories(libh7 PUBLIC h7/)
target_include_directories(libh7 PUBLIC ./)
target_link_libraries(libh7 PUBLIC m)

add_executable(h7_tests
    ${SRC_H7_TESTS}
    )
target_compile_definitions(h7_tests PUBLIC
    APE_TESTS_MAIN
    COLLECTIONS_DEBUG
    APE_DEBUG
    )
target_link_libraries(h7_tests PUBLIC libh7 m)
