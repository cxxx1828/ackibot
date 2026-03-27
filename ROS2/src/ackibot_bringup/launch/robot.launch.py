#!/usr/bin/env python3  # koristi Python 3 interpreter za izvršenje fajla

import os  # rad sa fajl sistemom
from ament_index_python.packages import get_package_share_directory  # vraća putanju do foldera paketa
from launch import LaunchDescription  # koristi se za kreiranje ROS2 launch opisa
from launch.actions import (
    DeclareLaunchArgument,  # deklaracija argumenta koji se može proslediti launch fajlu
    OpaqueFunction,  # omogućava pozivanje funkcije prilikom pokretanja launch fajla
    IncludeLaunchDescription  # uključivanje drugog launch fajla
)
from launch.launch_description_sources import PythonLaunchDescriptionSource  # za pokretanje Python launch fajlova
from launch.substitutions import (
    LaunchConfiguration,  # omogućava parametre koji se mogu menjati pri pokretanju launch fajla
    ThisLaunchFileDir,  # putanja do trenutnog launch fajla
    PathJoinSubstitution  # spajanje putanja
)
from launch_ros.actions import Node, SetRemap  # Node = ROS2 nod, SetRemap = remap topika
from launch_ros.substitutions import FindPackageShare  # pronalazi share folder paketa
from launch.conditions import IfCondition, UnlessCondition  # uslovi za uključivanje nodova ili launch fajlova

import ackibot_utils.uname  # modul za proveru sistema
from ackibot_utils.utils import show  # funkcija za ispis u konzolu
from ackibot_utils.usb_mapper import USB_Mapper  # pronalazi USB uređaje po klasi

def parse_bool(s):  # funkcija za parsiranje stringa u bool
    sl = s.lower()  # mala slova
    t = sl in ['true', '1', 'yes', 'y']  # lista true vrednosti
    f = sl in ['false', '0', 'no', 'n']  # lista false vrednosti
    if t:
        return True  # vraća True ako je u listi true vrednosti
    if f:
        return False  # vraća False ako je u listi false vrednosti
    raise TypeError(f'Cannot parse {s} to bool!')  # error ako nije prepoznato

def launch_setup(context, *args, **kwargs):
    en_teleop = LaunchConfiguration('en_teleop', default='true')  # parametar da li uključiti teleop, teleop znaci “teleoperation”, odnosno daljinsko upravljanje robotom
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')  # parametar za simulaciono vreme
    joypad = LaunchConfiguration('joypad', default='sony')  # koji joypad koristiti

    um = USB_Mapper()  # kreira instancu USB maper-a
    show(um.table)  # ispis svih USB uređaja
    arduino_port = um.get_exactly_1_dev_of_class('Arduino')  # pronalazi tačno jedan Arduino uređaj
    show(arduino_port)  # ispis porta Arduino uređaja

    params_fn = os.path.join(
        get_package_share_directory('ackibot_bringup'),
        'param',
        'ackibot.yaml'  # parametri za ackibot čvor
    )

    return [
        DeclareLaunchArgument(
            'use_sim_time',  # ime argumenta
            default_value=use_sim_time,  # podrazumevana vrednost
            description='Use simulation (Gazebo) clock if true'),  # opis argumenta
        DeclareLaunchArgument(
            'en_teleop',  # ime argumenta
            default_value=en_teleop,  # podrazumevana vrednost
            description='launch teleop'  # opis argumenta
        ),
        DeclareLaunchArgument(
            'joypad',  # ime argumenta
            default_value='sony'  # podrazumevana vrednost
        ),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                ThisLaunchFileDir(),  # direktorijum trenutnog launch fajla
                '/state_publisher.launch.py'  # fajl koji uključujemo
            ]),
            launch_arguments={'use_sim_time': use_sim_time}.items(),  # prosleđujemo argumente
        ),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                os.path.join(
                    get_package_share_directory('ackibot_teleop'),
                    'launch',
                    'teleop.launch.py'  # teleop launch fajl
                )
            ]),
            launch_arguments={
                'machine': 'sbc',  # tip mašine
                'joypad': joypad  # koji joypad koristiti
            }.items(),
            condition=IfCondition(en_teleop),  # uključi samo ako je en_teleop True
        ),

        Node(
            package="twist_stamper",  # paket twist_stamper
            executable="twist_stamper",  # izvršni fajl
            name='twist_stamper',  # ime nod-a
            output='screen',  # ispis u terminal
            remappings=[
                ('cmd_vel_in', '/cmd_vel_nav_unstamped'),  # ulazni topic
                ('cmd_vel_out', '/cmd_vel_nav_stamped'),  # izlazni topic
            ],
        ),

        Node(
            package="twist_mux",  # paket twist_mux
            executable="twist_mux",  # izvršni fajl
            name='twist_mux',  # ime nod-a
            output='screen',  # ispis u terminal
            parameters=[
                os.path.join(
                    get_package_share_directory('ackibot_bringup'),
                    'config',
                    'twist_mux_topics.yaml'  # YAML fajl sa topik i lock konfiguracijom
                ),
            ],
            remappings=[
                ('cmd_vel_out', '/cmd_vel_node'),  # izlaz remapovan na /cmd_vel_node
            ],
        ),

        Node(
            package='ackibot_node',  # glavni čvor robota
            executable='fw_node',  # izvršni fajl
            parameters=[params_fn],  # parametri iz ackibot.yaml
            arguments=['-i', arduino_port],  # prosleđujemo port Arduina
            output='screen',  # ispis u terminal
            remappings=[
                ('cmd_vel', '/cmd_vel_node'),  # ulaz komandnog topika
            ],
        ),
    ]

def generate_launch_description():
    ld = LaunchDescription([
        OpaqueFunction(function = launch_setup)  # pokreće funkciju za setup nodova
    ])
    
    return ld
