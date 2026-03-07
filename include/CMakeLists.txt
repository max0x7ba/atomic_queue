CMAKE_MINIMUM_REQUIRED( VERSION 3.25 )

include(GNUInstallDirs)

add_library(atomic_queue INTERFACE)
target_include_directories(atomic_queue INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

add_library(atomic_queue::atomic_queue ALIAS atomic_queue)

if ( ATOMIC_QUEUE_ENABLE_INSTALL )
    install(TARGETS atomic_queue EXPORT atomic_queue)
    install(
        DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/atomic_queue"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(
        EXPORT atomic_queue
        FILE atomic_queue-config.cmake
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/atomic_queue"
        NAMESPACE atomic_queue::
    )
endif ()
