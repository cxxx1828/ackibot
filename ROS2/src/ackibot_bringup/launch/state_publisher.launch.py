#!/usr/bin/env python3  # koristi Python 3 interpreter za izvršenje ovog fajla

# Ovaj launch fajl pokreće ROS2 čvor robot_state_publisher, koji je ključan za ROS2 sisteme jer objavljuje TF transformacije između delova robota na osnovu URDF modela.
# URDF (ackibot.urdf) opisuje sve linkove (delove) i zglobove robota.
# robot_state_publisher koristi URDF da izračuna gde se svaki link nalazi u prostoru i objavljuje ove transformacije kao TF frame-ove.
# TF frame-ovi se koriste u RViz-u, u kontrolerima, za odometriju i navigaciju.

import os  # omogućava rad sa fajl sistemom, npr. spajanje putanja

from ament_index_python.packages import get_package_share_directory  
# funkcija koja vraća putanju do "share" foldera ROS2 paketa

from launch import LaunchDescription  
# koristi se za kreiranje ROS2 launch opisa (lista nodova i akcija)

from launch.actions import DeclareLaunchArgument  
# omogućava deklaraciju argumenta koji se može proslediti launch fajlu

from launch.substitutions import LaunchConfiguration  
# omogućava kreiranje promenljivih koje se mogu menjati pri pokretanju launch fajla

from launch_ros.actions import Node  
# Node omogućava pokretanje ROS2 nodova iz launch fajla


def generate_launch_description():  
    # funkcija koju ROS2 poziva da dobije sve nodove i akcije za pokretanje

    use_sim_time = LaunchConfiguration('use_sim_time', default='false')  
    # parametar da li koristiti simulaciono vreme (Gazebo) ili stvarno vreme

    urdf_file_name = 'ackibot.urdf'  
    # ime URDF fajla koji opisuje geometriju i strukturu robota

    urdf = os.path.join(
        get_package_share_directory('ackibot_description'),  # putanja do share foldera paketa
        'urdf',  # folder u kojem se nalazi URDF fajl
        urdf_file_name)  # puni put do URDF fajla

    # Major refactor of the robot_state_publisher
    # Reference page: https://github.com/ros2/demos/pull/426
    # komentar o refaktorisanoj verziji robot_state_publisher-a

    with open(urdf, 'r') as infp:  
        # otvara URDF fajl u režimu čitanja
        robot_desc = infp.read()  
        # čita ceo sadržaj URDF fajla kao string

    rsp_params = {'robot_description': robot_desc}  
    # parametar koji se prosleđuje robot_state_publisher-u, sadrži URDF opis robota

    # print (robot_desc) # štampanje URDF informacija u terminal, opcionalno

    return LaunchDescription([  # vraća LaunchDescription koji ROS2 koristi da startuje nodove
        DeclareLaunchArgument(
            'use_sim_time',  # ime argumenta
            default_value='false',  # podrazumevana vrednost
            description='Use simulation (Gazebo) clock if true'  # opis argumenta
        ),
        Node(
            package='robot_state_publisher',  # ROS2 paket koji publikuje TF-ove robota
            executable='robot_state_publisher',  # izvršni fajl koji pokreće čvor
            output='screen',  # ispis logova u terminal
            parameters=[rsp_params, {'use_sim_time': use_sim_time}]  
            # parametri: URDF opis robota i flag za simulaciono vreme
        )
    ])
