//DIFERENCIJALNI POGON - kako se pokrecu dva tocka nezavisno jedan od drugog kako bi robot mogao da se okrece
//Inicijalizuje ROS cvor, priprema shared pointer, kerira odometriju

#include "diff_drive_controller.hpp"

#include <memory>

using ackibot::DiffDriveController;

//konstruktor klase DiffDriveController

//wheel separation - razmak izmedju tockova
//wheel radius - poluprecnik tocka
//poziva konstruktor Node klase, omogucava intra-process komunikaciju
DiffDriveController::DiffDriveController(const float wheel_seperation, const float wheel_radius)
: Node("diff_drive_controller", rclcpp::NodeOptions().use_intra_process_comms(true))
{
  //pametni pokazivac na Node objekat
  nh_ = std::shared_ptr<::rclcpp::Node>(this, [](::rclcpp::Node *) {});

   //objekt klase Odometry
  odometry_ = std::make_unique<Odometry>(
    nh_,                                        //pokazivac na Node objekat
    wheel_seperation,                         //razmak izmedju tockova            
    wheel_radius);              //poluprecnik tocka

  RCLCPP_INFO(this->get_logger(), "Run!");
}

