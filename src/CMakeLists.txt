CMAKE_MINIMUM_REQUIRED( VERSION 3.25 )

if ( ATOMIC_QUEUE_BUILD_EXAMPLES )
    add_executable(
        atomic_queue_example
        ${CMAKE_CURRENT_SOURCE_DIR}/example.cc
    )

    target_link_libraries(
        atomic_queue_example
        atomic_queue
    )

    add_test(
        NAME atomic_queue_example
        COMMAND atomic_queue_example
    )
endif()

if ( ATOMIC_QUEUE_BUILD_TESTS )
    find_package(Boost REQUIRED COMPONENTS unit_test_framework)

    add_executable(
        atomic_queue_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests.cc
    )

    target_link_libraries(
        atomic_queue_tests
        atomic_queue
        Boost::unit_test_framework
    )

    add_test(
        NAME atomic_queue_tests
        COMMAND atomic_queue_tests --log_level=unit_scope --report_level=short
    )
endif()

if ( ATOMIC_QUEUE_BUILD_BENCHMARKS_MIN )
    find_package(Boost REQUIRED COMPONENTS headers)

    add_executable(
        atomic_queue_benchmarks_min
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks_min.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/huge_pages.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu_base_frequency.cc
    )

    target_link_libraries(
        atomic_queue_benchmarks_min
        atomic_queue
        Boost::headers
    )

    add_test(
        NAME atomic_queue_benchmarks_min
        COMMAND atomic_queue_benchmarks_min
    )
endif()
