FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(cortex_tests
    test_types.cpp
)

target_link_libraries(cortex_tests PRIVATE
    cortex_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cortex_tests)
