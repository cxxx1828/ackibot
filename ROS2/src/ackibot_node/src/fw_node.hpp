// Author: Darby Lim
// Author: Milos Subotic

#pragma once

#include <array>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <tf2_ros/transform_broadcaster.h>

#include <geometry_msgs/msg/twist_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include <libserial/SerialPort.h>

#include "fw_pkgs.hpp"

class FW_Node : public rclcpp::Node {
public:
	typedef struct
	{
		float separation;		//razmak izmedju tockova
		float radius;		//poluprecnik tocka
	} Wheels;

	//parametri za kontrolu motora vezano za profil ubrzanja
	typedef struct
	{
		float profile_acceleration_constant;
		float profile_acceleration;
	} Motors;

	//konstruktor i destruktor
	explicit FW_Node(const std::string & usb_port);
	virtual ~FW_Node() {}

	//deklaracija metoda implementiranih u fw_node.cpp

	Wheels * get_wheels();

private:

	void publish_timer(const std::chrono::milliseconds timeout);

	void cmd_vel_callback();

	float enc_tick_per_rev;
	double tick_to_rad;

	//TODO Simplify
	Wheels wheels_;



	LibSerial::SerialPort motor_ctrl_sensor_hub_serial;


	i16 speed;           // brzina kretanja napred/nazad (za BLDC)
	i16 steering_angle;  // ugao skretanja volana (za servo)
	u8 watchdog_cnt;
	void watchdog_rst();
	void watchdog_dec();
	void watchdog_apply();

	rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr cmd_vel__sub;
	void cmd_vel__cb(const geometry_msgs::msg::TwistStamped::SharedPtr msg);
	geometry_msgs::msg::Twist prev_cmd;
	rclcpp::TimerBase::SharedPtr repeater__tmr;
	void repeater__cb();
	std::vector<u8> wr_buf;
	void write_pkg();


	std::thread read__thread;
	void read__loop();
	std::vector<u8> rd_buf;
	void read_pkg();
	i32 prev_enc;
	//i32 prev_enc[2];
	rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state__pub;

};
