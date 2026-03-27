
#define JS_READ_HZ 150

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joy.hpp>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>

#include <atomic>
#include <sstream>
#include <thread>
#include <chrono>

//DALJINSKO UPRAVLJANJE ROSOM

//cita podatke sa dzojstika i salje ih kao sensor_msgs/Joy poruke na /joy topic
//cuva stanje svih dugmadi i osa na dzojstiku


using namespace std;
using namespace std::chrono;

class Joy : public rclcpp::Node {
public:

//konstruktor klase joy
	Joy() 
		: 
		//konstruktor bazične klase Node, daje ime čvoru, inicijalizuju se promenljive
		Node("joy_node"),
		reading_state(false),			//da li čitamo džojstik
		js_fd(-1)						//broj fajl deskriptora
	{

		//postavljanje koordinatnog sistema, za joystick nije ni bitno
		joy_msg.header.frame_id = "joy"; //TODO What is anyway.


		int device_id;
		this->declare_parameter(
			"device_id",
			200
		);
		this->get_parameter<int>(
			"device_id",
			device_id
		);

		this->declare_parameter(
			"deadzone",
			0.1
		);
		this->get_parameter<float>(
			"deadzone",
			deadzone
		);

		float autorepeat_rate;
		this->declare_parameter(
			"autorepeat_rate",
			20.0
		);
		this->get_parameter<float>(
			"autorepeat_rate",
			autorepeat_rate
		);

		//output string stream
		ostringstream oss;
		//spaja se string putanje do joystika
		oss << "/dev/input/js" << device_id;
		//js_fn promenljiva koja cuva putanju do joystika
		js_fn = oss.str();
		

		//kreira se publisher unutar trenutnog noda
		joy__pub = this->create_publisher<sensor_msgs::msg::Joy>(
			"joy",									//naziv topica
			rclcpp::QoS(rclcpp::KeepLast(10))		//cuva se poslednjih 10 poruka u baferu, ako subscriber kasni dobice samo poslednjih 10 poruka jer se starije brisu
		);

		
		//binduje dole definisanu metodu sa niti
		//otvara dzojstik fajl i stalno cita dogadjaje sa njega
		js_fd__thread = thread(
			std::bind(&Joy::js_fd__task, this)
		);
		//kreira ROS2 tajmer
		//SALJE PORUKU AKO IMA NOVIH PODATAKA 
		poll_msg__timer = this->create_wall_timer(
			std::chrono::milliseconds(int(1000/JS_READ_HZ)),
			std::bind(&Joy::poll_msg__cb, this) //proverava da li ima nnovih podataka iz joystick niti
												//ako ima formira poruku i salje je pabliseru
												//ako nema novih podataka -> ne salje nista ili autorepeat
		);

		//SALJE PORUKU AKO JE OD POSLEDNJE PROMENE PROSLO DOVOLJNO VREMENA
		//ako je ukljucen autorepeat pravi se jos jedan tajmer
		if(autorepeat_rate > 0){
			autorepeat__timer = this->create_wall_timer(
				std::chrono::milliseconds(int(1000/autorepeat_rate)),
				std::bind(&Joy::autorepeat__cb, this)	//ponovo salje poslednju Joy poruku
			);
		}

	}

	~Joy(){

		// Close the device file
		close(js_fd);

		//TODO Kill thread.
		//js_fd__thread.join();
	}

protected:
	atomic_bool reading_state; //da li se cita joystick

	float deadzone;
	string js_fn;		//putanja do joysticka
	int js_fd;			//file descriptor za otvoreni joystick uredjaj

	std::thread js_fd__thread; 							//nit koja stalno cita joystick
	rclcpp::TimerBase::SharedPtr poll_msg__timer;		//tajmer koji proverava ima li novih dogadjaja
	rclcpp::TimerBase::SharedPtr autorepeat__timer;		//tajmer za autorepeat 
	
	std::mutex msg_mutex; // zastita new_msg & joy_msg kada im vise niti pristupa
	bool new_msg;							//flag da li postoji nova poruka za objavu
	sensor_msgs::msg::Joy joy_msg;			//sadrzi trenutnu vrednost joysticka, publishuje se na joy topic

	rclcpp::Publisher<sensor_msgs::msg::Joy>::SharedPtr joy__pub;		//publisher koji salje joy_msg na /joy

	std::chrono::time_point<std::chrono::steady_clock> t_prev;			//cuva vreme prethodne objave poruke


	//u publicu bindovana na nit
	//stalno radi i ima dve faze: opening state i reading state
	void js_fd__task() {
		while(true){
			if(!reading_state.load(std::memory_order_acquire)){
				//OPENING STATE
				//pokusava da otvori joystick uredjaj ako jos nije otvoren
				js_fd = open(js_fn.c_str(), O_RDONLY);
				if (js_fd == -1) {
					RCLCPP_ERROR_ONCE(
						this->get_logger(),
						"Error opening joystick dev \"%s\": errno %d %s",
						js_fn.c_str(),
						errno,
						strerror(errno)
					);
					this_thread::sleep_for(seconds(1));
				}else{
					RCLCPP_INFO(this->get_logger(), "Successful opening \"%s\"", js_fn.c_str());

					int N_axes, N_buttons;
					//cita broj osa i broj tastera
					ioctl(js_fd, JSIOCGAXES, &N_axes);
					ioctl(js_fd, JSIOCGBUTTONS, &N_buttons);
					
					//zastita da nit bezbedno menja joy_msg
					lock_guard<mutex> lock(msg_mutex);
					// Resize and set to 0
					joy_msg.axes.resize(N_axes, 0.0);
					joy_msg.buttons.resize(N_buttons, 0);

					//postavlja reading_state na true
					reading_state.store(true, std::memory_order_release);
				}
			}else{
				//READING STATE
				//stalno čita događaje sa joystick-a (js_event) i popunjava joy_msg

				struct js_event js_event_data;		//cuva jedan dogadjaj sa joysticka

				// Continuously read joystick events
				//fajl deskriptor, gde se cuva dogadjaj, velicina strukture
				//ako je procitana cela struktura onda je uspesno procitan dogadjaj
				if(read(js_fd, &js_event_data, sizeof(struct js_event)) == sizeof(struct js_event)){

					lock_guard<mutex> lock(msg_mutex);

					// Process the event based on its type
					//da li je pritisnuto dugme
					if(js_event_data.type & JS_EVENT_BUTTON){

						//buttons cuva sve trenutne vrednosti svih dugmadi na dzojstiku
						//u nizu buttons na poziciji broj dugmeta upisuje vrednost (0 ili 1)
						joy_msg.buttons[js_event_data.number] = js_event_data.value;
						//debug poruka
						//ispisuje koji je broj dugmeta pritisnut ili otpusten
						RCLCPP_DEBUG(
							this->get_logger(),
							"Button %d %s (value: %d)",
							js_event_data.number,
							(js_event_data.value == 0) ? "released" : "pressed",
							js_event_data.value
						);

					//da li je pomerena palica
					}else if(js_event_data.type & JS_EVENT_AXIS){
						// Normalize and invert.

						//normalizovana i invertovana vrednost da bi se dobila vrednost izmedju -1 i 1
						//invertovana je jer je na joysticku gore -1 a dole +1, na tastaturi suprotno
						float axis = -float(js_event_data.value) / 32767;

						//da li je apsolutna vrednost u deadzone, ako jeste onda se postavlja na 0
						if(abs(axis) < deadzone){
							axis = 0;
						}

						//niz axes cuva sve trenutne vrednosti svih osa na dzojstiku
						joy_msg.axes[js_event_data.number] = axis;

						RCLCPP_DEBUG(
							this->get_logger(),
							"Axis %d moved (value: %d)",
							js_event_data.number,
							js_event_data.value
						);

						//obradjuje incijalizaciju prilikom povezivanja dzojstika ili pokretanja programa
						//aplikacija na pocetku  zna u kom su stanju sva dugmad i palice
					}else if(js_event_data.type & JS_EVENT_INIT){
						RCLCPP_DEBUG(
							this->get_logger(),
							"Initial state event (type: %d, number: %d, value: %d)",
							js_event_data.type, js_event_data.number, js_event_data.value
						);

						//TODO What a heac is this? It is never called.
					}

					//ako je procitao bilo koju vrednost postavlja new_msg na true
					//znači da ima novu poruku za objavu
					new_msg = true;


				}else{
					RCLCPP_ERROR_ONCE(
						this->get_logger(),
						"Cannot read \"%s\": errno %d %s",
						js_fn.c_str(),
						errno,
						strerror(errno)
					);
					//TODO Test if joypad is plug out, and reading_state.store(false, std::memory_order_release);
				}

				this_thread::sleep_for(milliseconds(int(1/JS_READ_HZ)));
			}
		}
	}

	//bezbedni i pravovremeno objavljivanje stanja dzojstika
	void poll_msg__cb() {
		//ako je dozvoljeno citanje
		if(reading_state.load(std::memory_order_acquire)){

			lock_guard<mutex> lock(msg_mutex);
			new_msg = false; // trenutna poruka procitana i objavljena
			
			joy_msg.header.stamp = this->get_clock()->now();		//vreme objave poruke
			//TODO Why not publish from thread?
			joy__pub->publish(joy_msg);
			//TODO Do not publish from here if have autorepeat?
		}
	}

	//periodicno objavljivanje stanja dzojstika i kada nema novih dogadjaja
	void autorepeat__cb() {
		//ako je dozvoljeno citanje
		if(reading_state.load(std::memory_order_acquire)){

			lock_guard<mutex> lock(msg_mutex);

			joy_msg.header.stamp = this->get_clock()->now();
			joy__pub->publish(joy_msg);
		}
	}


};


int main(int argc, char * argv[]) {

	rclcpp::init(argc, argv);
	//kreira se ROS2 izvršilac
	rclcpp::executors::SingleThreadedExecutor executor;

	//kreira se čvor klase Joy
	auto joy_node = std::make_shared<Joy>();

	//čvor se dodaje u izvršilac i pokreće se
	executor.add_node(joy_node);
	//obradjuje sve dolazne dogadjaje dok se cvor ne ugasi
	executor.spin();

	//gasi se ROS2 i oslobadjaju resursi
	rclcpp::shutdown();
}