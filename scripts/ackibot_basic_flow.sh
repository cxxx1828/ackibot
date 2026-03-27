#!/bin/bash

exit 0

#export ROS2_DOMAIN=45

source /opt/ros/jazzy/setup.bash
export RCUTILS_COLORIZED_OUTPUT=1

# Build all.
colcon build --symlink-install
# Build specific package.
#colcon build --symlink-install --packages-select ackibot_node

source install/setup.sh

# Connect to joypad
./scripts/ackibot_joy.sh
# Press on white Bluetooth joypad R1 + Home

# Running ROS2 for robot
ros2 launch ackibot_bringup robot.launch.py

