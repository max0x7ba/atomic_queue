CMAKE_MINIMUM_REQUIRED( VERSION 3.25 )

PROJECT(atomic_queue VERSION 1.5.0)

OPTION( ATOMIC_QUEUE_BUILD_TESTS
    "If the tests should be built."
    OFF
)

OPTION( ATOMIC_QUEUE_BUILD_EXAMPLES
    "If examples should be built."
    OFF
)

if ( PROJECT_IS_TOP_LEVEL )
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED)
endif()

add_subdirectory( include )

if ( ATOMIC_QUEUE_BUILD_TESTS )
    enable_testing()
endif()

if ( ATOMIC_QUEUE_BUILD_TESTS OR ATOMIC_QUEUE_BUILD_EXAMPLES)
    add_subdirectory( src )
endif()

add_library(max0x7ba::atomic_queue ALIAS atomic_queue)