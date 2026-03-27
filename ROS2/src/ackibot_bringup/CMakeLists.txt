################################################################################
# Set minimum required version of cmake, project name and compile options
################################################################################
cmake_minimum_required(VERSION 3.5)  # minimalna verzija CMake-a koja je potrebna za ovaj projekat
project(ackibot_bringup)             # ime projekta/paketa

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)         # postavlja C++ standard na C++17 ako već nije definisan
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)  
  # aktivira sve osnovne upozoravajuće poruke kompajlera i pedantna upozorenja
endif()

################################################################################
# Find ament packages and libraries for ament and system dependencies
################################################################################
find_package(ament_cmake REQUIRED)  # pronalazi i učitava ament_cmake (ROS2 build sistem)

################################################################################
# Install
################################################################################
install(
  DIRECTORY launch param config   # direktorijumi koji se instaliraju
  DESTINATION share/${PROJECT_NAME}  # lokacija gde će se kopirati unutar instalacionog paketa
)

################################################################################
# Macro for ament package
################################################################################
ament_package()  # označava da je ovo ament paket i omogućava ROS2 build i instalaciju
