project(log_)

cmake_minimum_required(VERSION 3.13.0)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        .
)

target_sources(${PROJECT_NAME}
    INTERFACE
        ${PROJECT_NAME}.c
)
