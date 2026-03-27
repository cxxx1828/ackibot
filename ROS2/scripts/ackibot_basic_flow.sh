#!/bin/bash

exit 0

ackibot_joy.sh
# Press on white Bluetooth joypad R1 + Home
# Test joypad
ls -l /dev/input/js0
jstest /dev/input/js0

# Running ROS2 for robot
ackibot_run.sh

