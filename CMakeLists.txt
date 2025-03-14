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
find_package(OpenCV 4.5.4 REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)

# Enable Qt's Meta-Object Compiler (MOC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE DETECTION_SOURCES "src/detection/*.cpp")
file(GLOB_RECURSE DATABASE_SOURCES "src/database/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "src/network/*.cpp")

# UI sources (handled separately for Qt MOC)
set(UI_HEADERS
    "include/ui/main_window.hpp"
)

set(UI_SOURCES
    "src/ui/main_window.cpp"
)

# Common library for both CLI and GUI applications
add_library(hms_common STATIC
    ${CORE_SOURCES}
    ${DETECTION_SOURCES}
    ${DATABASE_SOURCES}
    ${NETWORK_SOURCES}
)

# Link common libraries to the common library
target_link_libraries(hms_common
    PRIVATE
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
    uuid
)

# CLI Application
add_executable(${PROJECT_NAME}_CLI
    src/cli/main.cpp
)

# Link libraries to CLI application
target_link_libraries(${PROJECT_NAME}_CLI
    PRIVATE
    hms_common
)

# GUI Application
add_executable(${PROJECT_NAME}_GUI
    src/gui/main_qt.cpp
    ${UI_SOURCES}
    ${UI_HEADERS}
)

# Link libraries to GUI application
target_link_libraries(${PROJECT_NAME}_GUI
    PRIVATE
    hms_common
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# Copy resources
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ 
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/)

# Create models directory and download YOLO model
add_custom_command(
    TARGET ${PROJECT_NAME}_GUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/download_models.sh
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Installation
install(TARGETS ${PROJECT_NAME}_CLI ${PROJECT_NAME}_GUI DESTINATION bin)
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/ DESTINATION bin/resources)
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/ DESTINATION bin/models)

# Testing
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
