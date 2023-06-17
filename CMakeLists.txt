project(log_)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        .
)

target_sources(${PROJECT_NAME}
    INTERFACE
        ${PROJECT_NAME}.c
)
