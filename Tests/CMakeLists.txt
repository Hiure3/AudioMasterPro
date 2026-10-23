list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(AMP_Tests
    test_db_utils.cpp
    test_dsp.cpp
    test_loudness.cpp
    test_engine.cpp
    test_misc.cpp)
target_include_directories(AMP_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AMP_Tests PRIVATE AMP_Core Catch2::Catch2WithMain)
catch_discover_tests(AMP_Tests)

# Teste de estabilidade (executavel proprio)
add_executable(AMP_StabilityTest stability_test.cpp)
target_link_libraries(AMP_StabilityTest PRIVATE AMP_Core)

# Rapido (20 s de audio simulado): roda sempre.
add_test(NAME stability_quick COMMAND AMP_StabilityTest --seconds 20)

# Longo (30 min de audio simulado): so com  ctest -C Release -L long
add_test(NAME stability_30min COMMAND AMP_StabilityTest --minutes 30)
set_tests_properties(stability_30min PROPERTIES LABELS long TIMEOUT 3600)
