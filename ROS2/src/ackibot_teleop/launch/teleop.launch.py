#!/usr/bin/env python3

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    OpaqueFunction,
)
from launch.substitutions import (
    LaunchConfiguration,
)
from launch_ros.actions import Node

import ackibot_utils.uname
from ackibot_utils.utils import show

def launch_setup(context, *args, **kwargs):
   
    machine = LaunchConfiguration('machine', default='')
    
    #dodaje sufiks imenu cvorova ili topica
    def add_suffix(name):
        p = machine.perform(context)
        if p == '':
            return name
        else:
            return '_'.join([name, p])

    joypad = LaunchConfiguration('joypad', default='sony')

    #trazi yaml fajl sa odgovarajucom konfiguracijom za dzojstik

    joypad_name = joypad.perform(context)
    def find_joypad_cfg(pkg):
        pkg_dir = get_package_share_directory(pkg)
        jc_file = os.path.join(
            pkg_dir,
            'config',
            f'{joypad_name}.config.yaml'
        )
        if os.path.exists(jc_file):
            return jc_file
        else:
            return None  

    joy_cfg_path = find_joypad_cfg('ackibot_teleop')
    if not joy_cfg_path:
        joy_cfg_path = find_joypad_cfg('teleop_twist_joy')
    show(joy_cfg_path)


    return [
        #argumenti koji se mogu proslediti prilikom pokretanja
        DeclareLaunchArgument(
            'machine',
            default_value=''
        ),

        DeclareLaunchArgument(
            'joypad',
            default_value='sony'
        ),

        #pokrece cvor za joy_node
        Node(
            #package='joy',
            package='ackibot_teleop',
            executable='joy_node',
            name=add_suffix('joy_node'),
            parameters=[{
                'device_id': 0,
                'deadzone': 0.3,
                'autorepeat_rate': 25.0,
                #'autorepeat_rate': 0.0, # Do not repeat.
            }],
            remappings = [
                ('/joy', add_suffix('joy')),
            ],
            respawn = True,
        ),
        #pokrece cvor za teleop_twist_joy
        Node(
            package='teleop_twist_joy',
            executable='teleop_node',
            name=add_suffix('teleop_twist_joy_node'),
            parameters=[
                joy_cfg_path,
                {'publish_stamped_twist': True}
            ],
            remappings = [
                ('/joy', add_suffix('joy')),
                ('/cmd_vel', add_suffix('cmd_vel_joy')),
            ],
        ),
    ]

def generate_launch_description():
	ld = LaunchDescription([
		OpaqueFunction(function = launch_setup)
	])

	return ld