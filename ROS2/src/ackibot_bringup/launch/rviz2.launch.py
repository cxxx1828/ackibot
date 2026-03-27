#!/usr/bin/env python3  # koristi Python 3 interpreter za izvršenje ovog fajla

import os  # omogućava rad sa fajl sistemom, npr. spajanje putanja

from ament_index_python.packages import get_package_share_directory  
# funkcija koja vraća putanju do "share" foldera nekog ROS2 paketa

from launch import LaunchDescription  
# koristi se za kreiranje ROS2 launch opisa (lista nodova i akcija)

from launch_ros.actions import Node  
# Node omogućava pokretanje ROS2 nodova iz launch fajla


def generate_launch_description():  
    # funkcija koju ROS2 poziva da dobije sve nodove i akcije za pokretanje
    rviz_config_file = os.path.join(
        get_package_share_directory('ackibot_description'),  # pronalazi share folder paketa 'ackibot_description'
        'rviz',  # folder u kojem se nalazi RViz konfiguracija
        'model.rviz'  # naziv RViz konfiguracionog fajla koji će se koristiti
    )

    return LaunchDescription([  # vraća LaunchDescription koji ROS2 koristi da startuje nodove
        Node(
            package='rviz2',  # ROS2 paket koji sadrži RViz čvor
            executable='rviz2',  # izvršni fajl koji pokreće RViz
            name='rviz2',  # ime nod-a unutar ROS2 sistema
            arguments=['-d', rviz_config_file],  
            # argumenti komandne linije: '-d' znači "load config file", a zatim path do .rviz fajla
            output='screen'),  # ispis logova i informacija nod-a prikazuje se u terminalu
    ])
