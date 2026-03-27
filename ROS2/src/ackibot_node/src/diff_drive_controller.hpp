
#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "odometry.hpp"

namespace ackibot
{
   //konstruktor klase DiffDriveController koji prima razmak izmedju tockova i poluprecnik tocka
class DiffDriveController : public rclcpp::Node
{
public:
  explicit DiffDriveController(const float wheel_seperation, const float wheel_radius);
  virtual ~DiffDriveController() {}

private:
  std::shared_ptr<rclcpp::Node> nh_;
  std::unique_ptr<Odometry> odometry_;
};
}  // namespace ackibot
