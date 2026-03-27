#include "fw_node.hpp"

#include <memory>
#include <string>

#include "sabertooth.h"
#include <rcpputils/asserts.hpp>


using namespace std::chrono_literals;

//ROS cvor koji povezuje Sabertooth motor kontroler sa ROS sistemom
//pretvara podatke u brzine motora, salje ih sabertoothu cita enkodere i objavljuje stanja tockova 
//na joint_states


#define REPEATER_HZ 25
#define WATCHDOG_TIMEOUT_PERIODS 3


#define L_WHEEL 0
#define R_WHEEL 1


#define DEBUG(var) do{ RCLCPP_INFO_STREAM(this->get_logger(), #var << " = " << var);  }while(0)
#define DEBUG_HEX(var) do{ RCLCPP_INFO_STREAM(this->get_logger(), #var << " = " << std::hex << var << std::dec);  }while(0)




FW_Node::FW_Node(const std::string & usb_port)
	: Node(
		"fw_node",
		rclcpp::NodeOptions().use_intra_process_comms(true)
	),
	//inicijalizacija clanova klase
	watchdog_cnt(0)
{
	RCLCPP_INFO(get_logger(), "Init FW_Node Node Main");

	prev_enc = 0;
	//prev_enc[1] = 0;

	//priprema bafera za slanje i prijem paketa
	wr_buf.resize(sizeof(pkg_m2s_t));
	rd_buf.resize(sizeof(pkg_s2m_t)-sizeof(pkg_magic_t));

	//debug logovanje velicina paketa
DEBUG(sizeof(pkg_m2s_t));
DEBUG(sizeof(pkg_s2m_t));

	//kreira publisher koji objavljuje poruke tipa sensor_msgs/JointState na topic joint_states
	joint_state__pub = this->create_publisher<sensor_msgs::msg::JointState>(
		"joint_states",
		rclcpp::QoS(rclcpp::KeepLast(10))
	);

	//objavljuje parametar cvora
	this->declare_parameter<float>("motors.enc_tick_per_rev");

	//pokkusava da ucita vrednost parametra
	this->get_parameter_or<float>(
		"motors.enc_tick_per_rev",
		enc_tick_per_rev,
		214.577
	);

	DEBUG(enc_tick_per_rev);

	//konverzija enkoder tikova u radijane
	tick_to_rad = 2*M_PI/enc_tick_per_rev;

	DEBUG(tick_to_rad);


	this->declare_parameter<float>("wheels.separation");
	this->declare_parameter<float>("wheels.radius");

	this->get_parameter_or<float>("wheels.separation", wheels_.separation, 0.160);
	this->get_parameter_or<float>("wheels.radius", wheels_.radius, 0.033);


	//pokusava da ostvari serijsku komunikaciju sa sabertoothom preko USB porta
	try{
		motor_ctrl_sensor_hub_serial.Open(usb_port);
		motor_ctrl_sensor_hub_serial.SetBaudRate(LibSerial::BaudRate::BAUD_115200);
		motor_ctrl_sensor_hub_serial.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
	}catch(...){
		RCLCPP_ERROR_STREAM(
			this->get_logger(),
			"Cannot open Sabertooth at \"" << usb_port << "\"!"
		);
		RCLCPP_INFO_STREAM(
			this->get_logger(),
			"Proceeding in powerless mode"
		);
	}


	//auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
	//subscribuje FW_Node na cmd_vel topik, prima poruke tipa geometry_msgs/TwistStamped
	//cmd_vel obicno sadrzi brzinu i ugaonu brzinu
	cmd_vel__sub = this->create_subscription<geometry_msgs::msg::TwistStamped>(
		"cmd_vel",
		1, //qos,
		std::bind(&FW_Node::cmd_vel__cb, this, std::placeholders::_1)
	);

	repeater__tmr = this->create_wall_timer(
		std::chrono::milliseconds(1000/REPEATER_HZ),
		std::bind(&FW_Node::repeater__cb, this)
	);


	//pokrece nit koja stalno cita pakete sa subskrajbera
	read__thread = std::thread(
		&FW_Node::read__loop,
		this
	);


	RCLCPP_INFO(this->get_logger(), "Run!");
}

FW_Node::Wheels * FW_Node::get_wheels() {
	return &wheels_;
}


void FW_Node::watchdog_rst() {
	watchdog_cnt = WATCHDOG_TIMEOUT_PERIODS;
}

void FW_Node::watchdog_dec() {
	if(watchdog_cnt != 0){
		watchdog_cnt--;
		if(watchdog_cnt == 0){
			RCLCPP_WARN_STREAM(this->get_logger(), "Watchdog stop motors!");
		}
	}
}

void FW_Node::watchdog_apply() {
	if(watchdog_cnt == 0){
		speed = 0;
		steering_angle = 90;
	}
}


//poziva se kada stigne nova poruka na cmd_vel topik
void FW_Node::cmd_vel__cb(const geometry_msgs::msg::TwistStamped::SharedPtr msg) {
	//pravi referencu na deo poruke koji sadrzi linearne i ugaone brzine
	geometry_msgs::msg::Twist& cmd = msg->twist;

	//da li je trenutna komanda razlicita od prethodne
	bool cmd_is_new = cmd != prev_cmd;
	prev_cmd = cmd;

	//ako je nova komanda ispisuje se na konzolu
	if(cmd_is_new){
		//RCLCPP_DEBUG(
		RCLCPP_INFO(
			this->get_logger(),
			"lin_vel: %f ang_vel: %f", cmd.linear.x, cmd.angular.z
		);
	}

	
	//PRETVARA NORMALIZOVANE BRZINE U VREDNOSTI ZA MOTORE (-2047, 2047)//////////////////////////////////////////////////////////////////////

	// [-1.0, 1.0] -> [reverse, forward]
	speed = static_cast<i16>(cmd.linear.x*2047);

	// [1.0, -1.0] -> [left, right].
	steering_angle = static_cast<i16>(((cmd.angular.z + 1.0)/2.0 * 180));  //mapira ugaonu brzinu na uglove od 0 do 180

	speed = 
		std::clamp(
			speed,
			i16(-MODULUS + 1), 
			i16(MODULUS - 1)
		);

	steering_angle = 
		std::clamp(
			steering_angle,
			i16(0),
			i16(180)
		);


	//CUVA VREDNOSTI BRZINA ZA LEVI I DESNI TOCAK
	//double motor[2]; // left, right

	//Preprocesorske direktive, if1 se uvek kompajlira, else uvek ignorise
// #if 1
// 	// Diff drive: steering in place.
// 	motor[L_WHEEL] = drive - turn;			//LEVI TOCAK SE SPORIJE KRECE KADA SE SKRECE U DESNO
// 	motor[R_WHEEL] = drive + turn;			//DESNI TOCAK SE SPORIJE KRECE KADA SE SKRECE U LEVO
// #else
// 	motor[L_WHEEL] = drive;
// 	motor[R_WHEEL] = drive;
// 	if(turn >= 0){
// 		motor[L_WHEEL] -= turn;
// 	}else{
// 		motor[R_WHEEL] -= turn;
// 	}
// #endif


	// //ogranicava vrednosti brzina na dozvoljeni opseg
	// for(int i = 0; i < 2; i++){
	// 	motor[i] = std::clamp(
	// 		motor[i],
	// 		-double(MODULUS-1),
	// 		+double(MODULUS-1)
	// 	);

	// 	speed[i] = motor[i];
	// }

#if 0
	RCLCPP_INFO(
		this->get_logger(),
		"motor[0]: %lf motor[1]: %lf", motor[0], motor[1]
	);
#endif

#if 0
	RCLCPP_INFO(
		this->get_logger(),
		"speed[0]: %d speed[1]: %d", speed[0], speed[1]
	);
#endif

	watchdog_rst();

	//FIXME This is bugy.
	//write_pkg();
}

//callback funkcija koja periodicno proverava i azurira stanje motora i ako nisu stigle nove komande
void FW_Node::repeater__cb() {
	watchdog_dec();
	watchdog_apply();

	write_pkg();
}

//salje trenutne brzine motora preko sabertootha
void FW_Node::write_pkg() {

	pkg_m2s_t& p = *reinterpret_cast<pkg_m2s_t*>(wr_buf.data());
	p.magic = PKG_MAGIC;
	//postavlja trenutne brzine motora u paket
	p.payload.speed = speed;
	p.payload.steering_angle = steering_angle;
	//postavlja rampu - koliko brzo motor treba da dostigne zadatu brzinu
	//p.payload.ramp_rate_ms = 2000; // TODO

	//racuna crc sumu za paket - procera integriteta podataka, da paket nije ostecen prilikom prenosa
	p.crc = CRC16().add(p.payload).get_crc();

	//ako je serijska veza otvorena salje paket
	if(motor_ctrl_sensor_hub_serial.IsOpen()){
		motor_ctrl_sensor_hub_serial.Write(
			wr_buf
		);
	}
}

//cita podatke sa sabertootha 
void FW_Node::read__loop() {
	//ako nije otvorena serijska veza, ne radi nista
	if(!motor_ctrl_sensor_hub_serial.IsOpen()){
		return;
	}
	while(true){
		read_pkg();
	}
}

void FW_Node::read_pkg() {
	//sluzi za sinhronizaciju paketa
	pkg_magic_t exp_magic = PKG_MAGIC;
	pkg_magic_t obs_magic = 0;
	
	//proverava da li paket pocinje sa ocekivanim magic brojem
	for(u8 i = 0; i < sizeof(pkg_magic_t); i++){
		try{
			motor_ctrl_sensor_hub_serial.Read(
				rd_buf,
				1,
				1 // [ms]
			);
		}catch(LibSerial::ReadTimeout& e){
			// Timeout.
			return;
		}

		//upisuje procitani bajt u obs_magic
		reinterpret_cast<u8*>(&obs_magic)[i] = rd_buf[0];
		//ako primlljeni bajt nije jednak ocekivanom magic bajtu, izlazi iz funkcije, paket je izgubljlen
		if(
			reinterpret_cast<u8*>(&exp_magic)[i] !=
			reinterpret_cast<u8*>(&obs_magic)[i]
		){
			// Lost magic.
			return;
		}
	}


	//cita ostatak paketa
	//ako ne uspe da procita u zadatom vremenu, izlazi iz funkcije
	try{
		motor_ctrl_sensor_hub_serial.Read(
			rd_buf,
			sizeof(pkg_s2m_t)-sizeof(pkg_magic_t),		//cita sve sem magicnih brojeva
			// Until all bytes are received.
			10*sizeof(pkg_s2m_t)*1000/9600*2+5 // [ms]	//odredjuje timeout
			//procenat greske * velicina paketa * ocekivano vreme prenosa u ms + mali dodatak

		);
	}catch(LibSerial::ReadTimeout& e){
		RCLCPP_WARN(this->get_logger(), "Cannot read sensor pkg!");
		return;
	}

	
	pkg_s2m_t p;				//struktura koja sadrzi primljeni paket
	p.magic = obs_magic;		//postavlja magic broj na osnovu primljenog
	//kopira ostatak paketa iz bafera u strukturu
	copy(
		rd_buf.begin(),
		rd_buf.end(),
		reinterpret_cast<uint8_t*>(&p) + sizeof(pkg_magic_t)
	);

	//proverava se integritet paketa preko CRC16 sume, ako ne odgovara paket je ostecen
	pkg_crc_t obs_crc = CRC16().add(p.payload).get_crc();
	if(obs_crc != p.crc){
		RCLCPP_WARN(this->get_logger(), "Wrong CRC!");
		DEBUG(obs_crc);
		DEBUG(p.crc);
//		return;
	}

	//promena znaka enkodera desnog tocka?????????????????????????????????????????????
	// Switch sign of 1 enc.
	// p.payload.enc[R_WHEEL] = -p.payload.enc[R_WHEEL];

	// // Only if changed.
	// //prikazuje enkodere samo ako su se promenili u odnosu na prethodnu vrednost
	// //sprecava spamovanje loga istim vrednostima
	// for(int i = 0; i < 2; i++){
	// 	if(p.payload.enc[i] != prev_enc[i]){
	// 		RCLCPP_INFO(
	// 			this->get_logger(), "L_enc = % 10d\tR_enc = % 10d",
	// 			p.payload.enc[L_WHEEL],
	// 			p.payload.enc[R_WHEEL]
	// 		);
	// 		break;
	// 	}
	// }
#if 0
	DEBUG(p.payload.speed_i[0]);
	DEBUG(p.payload.speed_i[1]);
	DEBUG(p.payload.speed_o[0]);
	DEBUG(p.payload.speed_o[1]);
#endif

	//cuva prethodne vrednosti enkodera
	// for(int i = 0; i < 2; i++){
	// 	prev_enc[i] = p.payload.enc[i];
	// }
	prev_enc = p.payload.enc;

	//publikuje procitane vrednosti enkodera na joint_states topik
	auto msg = std::make_unique<sensor_msgs::msg::JointState>();

	//postavlja zaglavlje ros poruke
	msg->header.frame_id = "base_link";
	msg->header.stamp = this->now();
	//postavlja imena zglobova
	//msg->name.push_back("wheel_left_joint");
	//msg->name.push_back("wheel_right_joint");
	msg->name.push_back("bldc_position");
	//pretvaraju enkoder tikove u rotaciju tockova i stavljaju u poruku da bi ostali cvorovi znali kako su tockovi okrenuti
	// msg->position.push_back(tick_to_rad * p.payload.enc[L_WHEEL]);
	// msg->position.push_back(tick_to_rad * p.payload.enc[R_WHEEL]);
	msg->position.push_back(tick_to_rad * p.payload.enc);


	joint_state__pub->publish(std::move(msg));
}

