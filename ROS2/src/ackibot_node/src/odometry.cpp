// Copyright 2019 ROBOTIS CO., LTD.
// Author: Darby Lim

#include "odometry.hpp"

#include <memory>
#include <string>
#include <utility>

using ackibot::Odometry;
using namespace std::chrono_literals;
//PUBLISHUJE NA ENC_ODOM
//SUBSCRIBUJE NA JOINT_STATES AKO IMU JESTE KORISTEN
//konstruktor koji prima tri parametra
Odometry::Odometry(
  std::shared_ptr<rclcpp::Node> & nh,   //pokazivac na ROS2 Node
  const double wheels_separation,       //rastojanje izmedju tockova
  const double wheels_radius)     //radijus tocka
: nh_(nh),
  wheels_separation_(wheels_separation),
  wheels_radius_(wheels_radius),
  use_imu_(false),
  publish_tf_(false),
  imu_angle_(0.0f)
  //inicijalizacija promenljivih
{
  //logovanje pocetka
  RCLCPP_INFO(nh_->get_logger(), "Init Odometry");

   //declare_parameter koristi se da ROS2 Node zna koje parametre može da učita.

  nh_->declare_parameter<std::string>("odometry.frame_id");   //ime koordinatnog sistema odometrije
  nh_->declare_parameter<std::string>("odometry.child_frame_id"); //ime koordinatnog sistema robota

  nh_->declare_parameter<bool>("odometry.use_imu");     //da li se koristi IMU
  nh_->declare_parameter<bool>("odometry.publish_tf");  //da li se salje TF transformacija

  //ucitavanje parametara ili postavljanje default vrednosti ako parametar nije definisan
  nh_->get_parameter_or<bool>(
    "odometry.use_imu",
    use_imu_,
    false);

  nh_->get_parameter_or<bool>(
    "odometry.publish_tf",
    publish_tf_,
    false);

  nh_->get_parameter_or<std::string>(
    "odometry.frame_id",
    frame_id_of_odometry_,
    std::string("odom"));

  nh_->get_parameter_or<std::string>(
    "odometry.child_frame_id",
    child_frame_id_of_odometry_,
    std::string("base_footprint"));

  
  //kreiranje publishera koji objavljuje odometriju
  auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
  odom_pub_ = nh_->create_publisher<nav_msgs::msg::Odometry>("/enc_odom", qos);

  tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(nh_);


	RCLCPP_WARN(nh_->get_logger(), "use_imu_ = %d", use_imu_);

///////////////////////// malo konteksta ////////////////////////////
//JOINT_STATE poruke sadrze informacije o poziciji i brzini tockova
//na osnovu toga se racuna odometrija pomocu diferencijalnog pogona:
//x,y,theta pozicija robota
//linearna i ugaona brzina
//
//IMU meri ugaonu brzinu, ubrzanje i orijentaciju
//koriguje greske koje dolaze od klizanja tockova i neravnog terena
////////////////////////////////////////////////////////////////////
 
if (use_imu_) {
    uint32_t queue_size = 10;

    //kreiranje sinhronizovanih subskrajbera za joint_state i IMU
    joint_state_imu_sync_ = std::make_shared<SynchronizerJointStateImu>(queue_size);

    //kreiranje message filter subskrajbera koji sluze za sinhronizaciju JointState i IMU poruka
    msg_ftr_joint_state_sub_ =
      std::make_shared<message_filters::Subscriber<sensor_msgs::msg::JointState>>(
      nh_,
      "joint_states");

    msg_ftr_imu_sub_ =
      std::make_shared<message_filters::Subscriber<sensor_msgs::msg::Imu>>(
      nh_,
      "imu");

    //povezuje dva pretplatnika sa sinhronizatorom, on moze da spaja vremenski najblize poruke
    joint_state_imu_sync_->connectInput(*msg_ftr_joint_state_sub_, *msg_ftr_imu_sub_);

    //duration - minimalno vreme izmedju poruka koje se mogu smatrati sinhronizovanim
    //ako poruka joint_state kasni  vise od 75ms u odnosu na IMU, nece biti uskladjene
    joint_state_imu_sync_->setInterMessageLowerBound(
      0,        //Prvi ulaz je joint_state
      rclcpp::Duration(75ms));

    joint_state_imu_sync_->setInterMessageLowerBound(
      1,              //Drugi ulaz je IMU
      rclcpp::Duration(15ms));

    //kada sinhronizator nadje par poruka koje su dovoljno blizu po vremenu, poziva se callback funkcija
    joint_state_imu_sync_->registerCallback(
      std::bind(
        &Odometry::joint_state_and_imu_callback,
        this,
        std::placeholders::_1,
        std::placeholders::_2));

  } else {
    //ako se ne koristi IMU, kreira se obican subskrajber za joint_state

    joint_state_sub_ = nh_->create_subscription<sensor_msgs::msg::JointState>(
      "joint_states",
      qos,
      std::bind(&Odometry::joint_state_callback, this, std::placeholders::_1));
      RCLCPP_WARN(nh_->get_logger(), "joint_states");

  }
}

//obradjuje joint_state poruke i racuna odometriju kada IMU nije koriscen
void Odometry::joint_state_callback(const sensor_msgs::msg::JointState::SharedPtr joint_state_msg)
{
  const rclcpp::Time current_time = joint_state_msg->header.stamp;  //vreme poruke
  static rclcpp::Time last_time = current_time;             //vreme prethodne poruke  
  const rclcpp::Duration duration = current_time - last_time;     //vremenski interval izmedju dve poruke

  update_joint_state(joint_state_msg);
  calculate_odometry(duration);
  publish(current_time);        //publishuje poruku odometrije i TF transformaciju

  last_time = current_time;
}

//obradjuje poruke joint_state i IMU i racuna odometriju
void Odometry::joint_state_and_imu_callback(
  const std::shared_ptr<sensor_msgs::msg::JointState const> & joint_state_msg,
  const std::shared_ptr<sensor_msgs::msg::Imu const> & imu_msg)
{
  RCLCPP_DEBUG(
    nh_->get_logger(),
    "[joint_state_msg_] nanosec : %d [imu_msg] nanosec : %d",
    joint_state_msg->header.stamp.nanosec,
    imu_msg->header.stamp.nanosec);

  const rclcpp::Time current_time = joint_state_msg->header.stamp;
  static rclcpp::Time last_time = current_time;
  const rclcpp::Duration duration = current_time - last_time;

  update_joint_state(joint_state_msg);
  update_imu(imu_msg);
  calculate_odometry(duration);
  publish(current_time);

  last_time = current_time;
}

//slanje poruke odometrije i TF transformacije
void Odometry::publish(const rclcpp::Time & now)
{
  auto odom_msg = std::make_unique<nav_msgs::msg::Odometry>();  //kreira se poruka odometrije

  odom_msg->header.frame_id = frame_id_of_odometry_;
  odom_msg->child_frame_id = child_frame_id_of_odometry_;
  odom_msg->header.stamp = now;

  //pozicija robota
  odom_msg->pose.pose.position.x = robot_pose_[0];    //x
  odom_msg->pose.pose.position.y = robot_pose_[1];    //y
  odom_msg->pose.pose.position.z = 0;  //z uvek 0 jer se robot krece po ravni


  //orijentacija robota se predstavlja kvaternionom
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, robot_pose_[2]);       //theta

  //postavljanje kvaterniona u poruci odometrije

  odom_msg->pose.pose.orientation.x = q.x();
  odom_msg->pose.pose.orientation.y = q.y();
  odom_msg->pose.pose.orientation.z = q.z();
  odom_msg->pose.pose.orientation.w = q.w();

  //postavljanje brzine robota u poruci odometrije

  odom_msg->twist.twist.linear.x = robot_vel_[0];
  odom_msg->twist.twist.angular.z = robot_vel_[2];

  // TODO(Will Son): Find more accurate covariance.
  // odom_msg->pose.covariance[0] = 0.05;
  // odom_msg->pose.covariance[7] = 0.05;
  // odom_msg->pose.covariance[14] = 1.0e-9;
  // odom_msg->pose.covariance[21] = 1.0e-9;
  // odom_msg->pose.covariance[28] = 1.0e-9;
  // odom_msg->pose.covariance[35] = 0.0872665;

  // odom_msg->twist.covariance[0] = 0.001;
  // odom_msg->twist.covariance[7] = 1.0e-9;
  // odom_msg->twist.covariance[14] = 1.0e-9;
  // odom_msg->twist.covariance[21] = 1.0e-9;
  // odom_msg->twist.covariance[28] = 1.0e-9;
  // odom_msg->twist.covariance[35] = 0.001;

  //kreira se TF transformacija izmedju koordinatnih sistema odometrije i robota

  geometry_msgs::msg::TransformStamped odom_tf;

//popunjavanje TF transformacije podacima iz poruke odometrije
  odom_tf.transform.translation.x = odom_msg->pose.pose.position.x;
  odom_tf.transform.translation.y = odom_msg->pose.pose.position.y;
  odom_tf.transform.translation.z = odom_msg->pose.pose.position.z;
  odom_tf.transform.rotation = odom_msg->pose.pose.orientation;

//definisanje zaglavlja TF transformacije i vremena
  odom_tf.header.frame_id = frame_id_of_odometry_;
  odom_tf.child_frame_id = child_frame_id_of_odometry_;
  odom_tf.header.stamp = now;

//publishovanje poruke odometrije
  odom_pub_->publish(std::move(odom_msg));

//slanje TF transformacije ako je publish_tf_ true
  if (publish_tf_) {
    tf_broadcaster_->sendTransform(odom_tf);
  }
}

//prati promenu pozicija tockova i racuna razliku u odnosu na prethodnu poziciju
void Odometry::update_joint_state(
  const std::shared_ptr<sensor_msgs::msg::JointState const> & joint_state)
{
  static std::array<double, 2> last_joint_positions = {0.0f, 0.0f};

  diff_joint_positions_[0] = joint_state->position[0] - last_joint_positions[0];
  diff_joint_positions_[1] = joint_state->position[1] - last_joint_positions[1];

  last_joint_positions[0] = joint_state->position[0];
  last_joint_positions[1] = joint_state->position[1];
}

//koristi podatke sa IMU senzora da azurira ugao rotacije robota oko z ose
void Odometry::update_imu(const std::shared_ptr<sensor_msgs::msg::Imu const> & imu)
{
  imu_angle_ = atan2f(
    imu->orientation.x * imu->orientation.y + imu->orientation.w * imu->orientation.z,
    0.5f - imu->orientation.y * imu->orientation.y - imu->orientation.z * imu->orientation.z);
}

//prima vremenski interval izmedju dve poruke
bool Odometry::calculate_odometry(const rclcpp::Duration & duration)
{
  //razlika u poziciji tockova od prethodne poruke
  double wheel_l = diff_joint_positions_[0];
  double wheel_r = diff_joint_positions_[1];

  double delta_s = 0.0;
  double delta_theta = 0.0;

  double theta = 0.0;
  static double last_theta = 0.0;

  double v = 0.0;
  double w = 0.0;

  double step_time = duration.seconds();

  //provera vremenskog intervala, vrednosti tockova
  if (step_time == 0.0) {
    return false;
  }

  if (std::isnan(wheel_l)) {
    wheel_l = 0.0;
  }

  if (std::isnan(wheel_r)) {
    wheel_r = 0.0;
  }

  //predjena udaljenost centra robota - prosecna vrednost oba tocka * radijus
  delta_s = wheels_radius_ * (wheel_r + wheel_l) / 2.0;

  // racunanje promene ugla theta sa ili bez IMU
  if (use_imu_) {
    theta = imu_angle_;
    delta_theta = theta - last_theta;
  } else {
    //ako nema IMU racuna se ugao na osnovu razlike u poziciji tockova
    theta = wheels_radius_ * (wheel_r - wheel_l) / wheels_separation_;

    delta_theta = theta;
  }


 // azuriranje pozicije robota
  robot_pose_[0] += delta_s * cos(robot_pose_[2] + (delta_theta / 2.0));
  robot_pose_[1] += delta_s * sin(robot_pose_[2] + (delta_theta / 2.0));
  robot_pose_[2] += delta_theta;

  // racunanje linearne i ugaone brzine
  v = delta_s / step_time;
  w = delta_theta / step_time;

  robot_vel_[0] = v;
  robot_vel_[1] = 0.0;    //brzina po y je uvek 0
  robot_vel_[2] = w;

  last_theta = theta;
  return true;
}
