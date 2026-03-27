################################################################################
# Set minimum required version of cmake, project name and compile options
################################################################################
cmake_minimum_required(VERSION 3.5)
project(ackibot_node)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

################################################################################
# Find ament packages and libraries for ament and system dependencies
################################################################################
find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(message_filters REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcutils REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

# Finds the pkg-config executable, adds the pkg_check_modules() function, among others.
find_package(PkgConfig REQUIRED)
# Create imported target with name PkgConfig::LIBSERIAL
pkg_check_modules(LIBSERIAL REQUIRED IMPORTED_TARGET libserial)

################################################################################
# Build
################################################################################
include_directories(
  include
)

set(DEPENDENCIES
  "geometry_msgs"
  "message_filters"
  "nav_msgs"
  "rclcpp"
  "rcutils"
  "sensor_msgs"
  "std_msgs"
  "std_srvs"
  "tf2"
  "tf2_ros"
)

set(EXECUTABLE_NAME "fw_node")

add_executable(
  ${EXECUTABLE_NAME}
  src/diff_drive_controller.cpp
  src/odometry.cpp
  src/fw_node.cpp
  src/main.cpp
)

target_link_libraries(${EXECUTABLE_NAME} "${LIBSERIAL_LIBRARIES}")
ament_target_dependencies(${EXECUTABLE_NAME} ${DEPENDENCIES})

################################################################################
# Install
################################################################################

install(TARGETS ${EXECUTABLE_NAME}
  DESTINATION lib/${PROJECT_NAME}
)

################################################################################
# Macro for ament package
################################################################################
ament_export_include_directories(include)
ament_package()
