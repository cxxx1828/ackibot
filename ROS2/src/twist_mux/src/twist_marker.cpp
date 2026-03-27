// Copyright 2020 PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the PAL Robotics S.L. nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/*
 * @author Enrique Fernandez
 * @author Jeremie Deray
 * @author Brighten Lee
 */

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

#include <memory>
#include <string>

/**
 * Ovo je ROS2 C++ čvor (node) koji:
 *  - prima poruke tipa geometry_msgs::msg::Twist ili geometry_msgs::msg::TwistStamped (linearna i ugaona brzina),
 *  - pretvara ih u vizualizacione strelice (visualization_msgs::msg::Marker),
 *  - i šalje ih na ROS topic da bi se mogle prikazati npr. u RViz-u.
*/

// Ova klasa enkapsulira jedan Marker (strelicu u RViz-u)
class TwistMarker
{
public:
  /**
   * frame_id - koordinatni sistem u kojem će se marker crtati (npr. base_footprint).
   * scale - skala (veličina) markera.
   * z - vertikalna pozicija.
   */
  TwistMarker(std::string & frame_id, double scale, double z)
  : frame_id_(frame_id), scale_(scale), z_(z)
  {
    // ID and type:
    marker_.id = 0;
    marker_.type = visualization_msgs::msg::Marker::ARROW;

    // Frame ID:
    marker_.header.frame_id = frame_id_;

    // Pre-allocate points for setting the arrow with the twist:
    marker_.points.resize(2); // pocetak i kraj strelice

    // Vertical position:
    marker_.pose.position.z = z_;

    // Scale:
    marker_.scale.x = 0.05 * scale_;
    marker_.scale.y = 2 * marker_.scale.x;

    // Color:
    marker_.color.a = 1.0; // Predstavlja providnost (1.0 znaci potpuno vidljivo)
    marker_.color.r = 0.0;
    marker_.color.g = 1.0;
    marker_.color.b = 0.0;

    // Error when all points are zero:
    marker_.points[1].z = 0.01;
  }

  // Ažurira se dužina i orijentacija strelice na osnovu ulaznog twist
  void update(const geometry_msgs::msg::Twist & twist)
  {
    using std::abs;

    // Strelica u X pravcu zavisi od linearne brzine naprijed/nazad
    marker_.points[1].x = twist.linear.x;

    // U Y pravcu biramo između bočne brzine (linear.y) i ugaone brzine (angular.z)
    // !! za holonomske robote može imati smisla, za aute linear.y = 0
    if (abs(twist.linear.y) > abs(twist.angular.z)) {
      marker_.points[1].y = twist.linear.y;
    } else {
      marker_.points[1].y = twist.angular.z;
    }
  }

  // Getter za marker (koristi publisher kasnije)
  const visualization_msgs::msg::Marker & getMarker()
  {
    return marker_;
  }

private:
  visualization_msgs::msg::Marker marker_; // Objekt koji šaljemo u RViz

  std::string frame_id_;
  double scale_;
  double z_;
};

/**
 * Ovo je ROS2 čvor koji:
 *  - Čita parametre (frame_id, scale, use_stamped, vertical_position).
 *  - Kreira instancu TwistMarker.
 *  - Postavlja subscriber na twist topic:
 *      - Ako je use_stamped = true → koristi geometry_msgs::msg::TwistStamped.
 *      - Inače koristi obični geometry_msgs::msg::Twist.
 *  - Kreira publisher za visualization_msgs::msg::Marker na topicu marker.
 */
class TwistMarkerPublisher : public rclcpp::Node
{
public:
  TwistMarkerPublisher()
  : Node("twist_marker")
  {
    std::string frame_id;
    double scale;
    bool use_stamped = true;
    double z;

    // Deklaracija ROS parametara (sa default vrijednostima)
    this->declare_parameter("frame_id", "base_footprint");
    this->declare_parameter("scale", 1.0);
    this->declare_parameter("use_stamped", true);
    this->declare_parameter("vertical_position", 2.0);

    // Čitanje parametara u varijable
    this->get_parameter<std::string>("frame_id", frame_id);
    this->get_parameter<double>("scale", scale);
    this->get_parameter<bool>("use_stamped", use_stamped);
    this->get_parameter<double>("vertical_position", z);

    // Inicijalizacija markera
    marker_ = std::make_shared<TwistMarker>(frame_id, scale, z);

    // Subscriber na topic "twist"
    if (use_stamped)
    {
      sub_stamped_ = this->create_subscription<geometry_msgs::msg::TwistStamped>(
        "twist", rclcpp::SystemDefaultsQoS(),
        std::bind(&TwistMarkerPublisher::callback_stamped, this, std::placeholders::_1));
    }
    else
    {
      sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
        "twist", rclcpp::SystemDefaultsQoS(),
        std::bind(&TwistMarkerPublisher::callback, this, std::placeholders::_1));
    }

    // Publisher za marker
    pub_ =
      this->create_publisher<visualization_msgs::msg::Marker>(
      "marker",
      rclcpp::QoS(rclcpp::KeepLast(1)));
  }

  // Callback kada stigne obični Twist
  void callback(const geometry_msgs::msg::Twist::ConstSharedPtr twist)
  {
    marker_->update(*twist);

    pub_->publish(marker_->getMarker());
  }

  // Callback kada stigne TwistStamped
  void callback_stamped(const geometry_msgs::msg::TwistStamped::ConstSharedPtr twist)
  {
    marker_->update(twist->twist);

    pub_->publish(marker_->getMarker());
  }

private:
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_; // subscriber za Twist
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr sub_stamped_; // subscriber za TwistStamped
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr pub_; // publisher markera

  std::shared_ptr<TwistMarker> marker_ = nullptr;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv); // pokretanje ROS2

  auto twist_mux_node = std::make_shared<TwistMarkerPublisher>();

  rclcpp::spin(twist_mux_node); // Cvor radi i osluškuje poruke, beskonacna petlja

  rclcpp::shutdown(); // Uredno gašenje

  return EXIT_SUCCESS;
}
