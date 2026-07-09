include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(unit_tests
    test_cache.cpp
    test_hierarchy.cpp
    test_ringbuffer.cpp
    test_processor.cpp
)
target_link_libraries(unit_tests PRIVATE packet_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(unit_tests)

# Integration tests: generate the standard traces once, then assert the
# published numbers. These are the same figures quoted in the README, so CI
# fails if the results tables ever drift from the code.
add_test(NAME traces_setup COMMAND $<TARGET_FILE:tracegen> --all)
set_tests_properties(traces_setup PROPERTIES FIXTURES_SETUP traces)

add_test(NAME golden_workingset_lru
         COMMAND $<TARGET_FILE:cache_sim> trace_workingset.txt --policy lru)
set_tests_properties(golden_workingset_lru PROPERTIES
    FIXTURES_REQUIRED traces
    PASS_REGULAR_EXPRESSION "AMAT: 16.288 cycles")

add_test(NAME golden_workingset_random
         COMMAND $<TARGET_FILE:cache_sim> trace_workingset.txt --policy random)
set_tests_properties(golden_workingset_random PROPERTIES
    FIXTURES_REQUIRED traces
    PASS_REGULAR_EXPRESSION "AMAT: 12.2992 cycles")

add_test(NAME golden_workingset_fits
         COMMAND $<TARGET_FILE:cache_sim> trace_workingset_fits.txt --policy lru)
set_tests_properties(golden_workingset_fits PROPERTIES
    FIXTURES_REQUIRED traces
    PASS_REGULAR_EXPRESSION "AMAT: 4.2016 cycles")

add_test(NAME malformed_lines_reported
         COMMAND $<TARGET_FILE:cache_sim>
                 ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/malformed_trace.txt --policy lru)
set_tests_properties(malformed_lines_reported PROPERTIES
    PASS_REGULAR_EXPRESSION "skipped 3 malformed lines")

add_test(NAME packet_smoke_setup
         COMMAND $<TARGET_FILE:packetgen> mixed 2000 1000 pkt_smoke.txt)
set_tests_properties(packet_smoke_setup PROPERTIES FIXTURES_SETUP pkttrace)

add_test(NAME packet_smoke
         COMMAND $<TARGET_FILE:packet_sim> pkt_smoke.txt 8)
set_tests_properties(packet_smoke PROPERTIES
    FIXTURES_REQUIRED pkttrace
    PASS_REGULAR_EXPRESSION "Packets arrived:   2000")
