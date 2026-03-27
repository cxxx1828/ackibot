################################################################################
# Set minimum required version of cmake, project name and compile options
################################################################################
cmake_minimum_required(VERSION 3.8)  # Minimalna verzija CMake-a potrebna za izgradnju paketa
project(ackibot_utils)  # Ime projekta/paketa je ackibot_utils

################################################################################
# Find ament packages and libraries for ament and system dependencies
################################################################################
find_package(ament_cmake REQUIRED)  # Pronalazi ament_cmake paket, neophodan za ROS2 CMake pakete
find_package(ament_cmake_python REQUIRED)  # Pronalazi ament_cmake_python paket za instalaciju Python paketa

################################################################################
# Build
################################################################################
# Ovde bi išli eventualni build koraci za C++ kod, ali pošto je ovo Python paket, nije potrebno

#############
## Install ##
#############

# Install Python modules
ament_python_install_package(ackibot_utils)  # Instalira Python paket ackibot_utils u ROS2 workspace-u

# Definiše ovaj paket kao ament paket da ga ROS2 može prepoznati
ament_package()  
