import os  # omogućava rad sa fajl sistemom

from ament_index_python.packages import get_package_share_directory  # funkcija koja vraća putanju do foldera paketa
from launch import LaunchDescription  # koristi se za kreiranje ROS2 launch opisa
from launch_ros.actions import Node  # omogućava pokretanje ROS2 nodova
from launch.substitutions import (
    LaunchConfiguration,  # omogućava parametre koji se mogu menjati pri pokretanju launch fajla
)
from launch.actions import (
    DeclareLaunchArgument,  # deklaracija argumenta koji se može proslediti launch fajlu
    OpaqueFunction,  # omogućava pozivanje funkcije prilikom pokretanja launch fajla
)

from ackibot_utils.utils import show  # funkcija za ispis informacija u konzolu (debug)

def launch_setup(context, *args, **kwargs):
    serial_port = LaunchConfiguration('serial_port', default='/dev/ttyUSB2')  
    # serial_port je parametar koji određuje koji USB port koristi BNO055 senzor

    return [
        DeclareLaunchArgument(
            'serial_port',  # ime argumenta
            default_value=serial_port,  # podrazumevana vrednost USB porta
            description='USB device for IMU'  # opis argumenta
        ),

        Node(
            package='bno055',  # paket koji sadrži driver za BNO055 IMU
            executable='bno055',  # izvršni fajl koji pokreće čitanje sa senzora
            respawn=True,  # ako nod padne, automatski se restartuje
            parameters=[
                os.path.join(
                    get_package_share_directory('bno055'),
                    'config',
                    'bno055_params.yaml'  # konfiguracioni fajl sa parametrima senzora
                ),
                {
                    'uart_port': serial_port,# port koji BNO055 node koristi; ovo prepisuje (override) vrednost iz YAML fajla sa vrednošću iz launch fajla
                }
            ],
            remappings=[
                ('/bno055/imu', '/imu')  # remapuje izlaz senzora na temu /imu
            ],
            # Tok podataka: BNO055 -> ROS2 topic /imu -> ekf_node (fusion sa odometrijom) -> tf-ovi oni/transformacije i RViz
        ),

        Node(
            package='robot_localization',  # paket za fuziju senzorskih podataka
            executable='ekf_node',  # EKF čvor koji kombinuje IMU + odometriju
            name='ekf_node',  # ime nod-a
            output='screen',  # ispis u terminal
            parameters=[
                os.path.join(
                    get_package_share_directory('ackibot_bringup'), 
                    'config/ekf.yaml'  # EKF konfiguracija, koje teme i ose koristi
                ),
            ],
            remappings=[
                ('/odometry/filtered', '/odom')  # izlaz odometrije remapovan na /odom
            ],
            # Tok podataka: /imu + odom -> ekf_node -> filtered odometry -> /odom
            # EKF kombinuje ubrzanja, rotacije i odometriju da bi robot imao preciznu poziciju u prostoru
        ),
    ]

def generate_launch_description():
    ld = LaunchDescription([
        OpaqueFunction(function=launch_setup)  # poziva gore definisanu funkciju za setup nodova
    ])
    
    return ld
