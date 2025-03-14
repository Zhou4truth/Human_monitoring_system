cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(OpenCV 4.5.4 REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Add test executables
add_executable(test_database test_database.cpp)
target_link_libraries(test_database
    PRIVATE
    hms_common
    ${SQLite3_LIBRARIES}
    ${Boost_LIBRARIES}
)

add_executable(test_human_detector test_human_detector.cpp)
target_link_libraries(test_human_detector
    PRIVATE
    hms_common
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)

add_executable(test_fall_detector test_fall_detector.cpp)
target_link_libraries(test_fall_detector
    PRIVATE
    hms_common
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)

add_executable(test_notification test_notification.cpp)
target_link_libraries(test_notification
    PRIVATE
    hms_common
    ${CURL_LIBRARIES}
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Add tests
add_test(NAME DatabaseTest COMMAND test_database)
add_test(NAME HumanDetectorTest COMMAND test_human_detector)
add_test(NAME FallDetectorTest COMMAND test_fall_detector)
add_test(NAME NotificationTest COMMAND test_notification)
