cmake_minimum_required(VERSION 3.16)
project(HumanMonitoringSystem VERSION 1.0.0 LANGUAGES CXX)

# Configure C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(OpenCV 4.7 REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE UI_SOURCES "src/ui/*.cpp")
file(GLOB_RECURSE DETECTION_SOURCES "src/detection/*.cpp")
file(GLOB_RECURSE DATABASE_SOURCES "src/database/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "src/network/*.cpp")

# Define the executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${DETECTION_SOURCES}
    ${DATABASE_SOURCES}
    ${NETWORK_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
)

# Copy resources
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ 
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/)

# Add YOLO weights download script
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/download_models.sh
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/ DESTINATION bin/resources)
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/ DESTINATION bin/models)

# Testing
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
