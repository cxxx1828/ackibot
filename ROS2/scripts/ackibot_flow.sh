#!/bin/bash

exit 0



ros2 run teleop_twist_joy teleop_node --ros-args \
    --params-file `ros2 pkg prefix --share  ackibot_teleop`/config/sony.config.yaml \
    -p publish_stamped_twist:=true \
    -r joy:=/joy_sbc \
    -r cmd_vel:=/cmd_vel_joy_sbc \

