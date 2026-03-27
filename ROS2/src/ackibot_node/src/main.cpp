
#include <chrono>
#include <memory>
#include <string>

#include <rcutils/cmdline_parser.h>
#include <rclcpp/rclcpp.hpp>

#include "diff_drive_controller.hpp"
#include "fw_node.hpp"

//samo pokrece cvorove ali oni unutar sebe rade publish i subscribe
//upustva koja se ispisuju na konzoli
void help_print()
{
	printf("For ACKIbot ctrl node : \n");
	printf("ctrl_node [-i usb_port] [-h]\n");
	printf("options:\n");
	printf("-h : Print this help function.\n");
	printf("-i usb_port: Connected USB port with Sabertooth motor drv.");
}

int main(int argc, char * argv[])
{
	//konfigurise baffering za standardni izlaz
	//postavlja se na _IONBF sto znaci da nema buffera, svaki izlaz se odmah ispisuje
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	//da li je program pokrenut sa opcijom -h -> ispisuje se help
	if (rcutils_cli_option_exist(argv, argv + argc, "-h")) {
		help_print();
		return 0;
	}

	//inicijalizacija ROS2
	rclcpp::init(argc, argv);

	std::string usb_port = "/dev/ttyUSB0";		//default port koji se koristi ako nije prosledjen drugi
	char * cli_options;							//cuva vrednost opcije -i
	cli_options = rcutils_cli_get_option(argv, argv + argc, "-i");			//cita vrednost opcije -i ako je ima
	if (nullptr != cli_options) {
		usb_port = std::string(cli_options);
	}

	//kreira se SingleThreadedExecutor koji ce da upravlja cvorovima
	rclcpp::executors::SingleThreadedExecutor executor;

	//kreira se cvor za komunikaciju sa hardverom
	auto fw_node = std::make_shared<FW_Node>(usb_port);

	//kreira se cvor za kontrolu diferencijalne voznje
	auto diff_drive_controller =
		std::make_shared<ackibot::DiffDriveController>(
		fw_node->get_wheels()->separation,
		fw_node->get_wheels()->radius);

	//dodaju se cvorovi u executor i pokrece se spin
	executor.add_node(fw_node);
	executor.add_node(diff_drive_controller);
	executor.spin();

	rclcpp::shutdown();

	return 0;
}
