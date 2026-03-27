from pyudev import Context, Device  # import modula za pristupanje uređajima preko udev sistema na Linux-u
import glob  # modul za pronalaženje fajlova po šablonima (wildcards)

# Kod je ROS/embedded helper za automatsko prepoznavanje USB uređaja na Linux-u (npr. Raspberry Pi).
# Umesto da ručno kucaš /dev/ttyUSB0 ili /dev/ttyACM0, ovaj kod detektuje:
# Arduino uređaje
# LIDAR
# IMU
# Debug UART
# i automatski ih mapira po klasama.


# Mapa koja povezuje VID:PID USB uređaja sa klasama uređaja
vid_pid__2_class__map = {
    '1a86:7523' : ['Arduino'],  # QinHeng CH340 USB-Serial konverter, klasifikovan kao Arduino
    '0403:6015' : ['Arduino'],  # FTDI USB-Bridge, klasifikovan kao Arduino
    '10c4:ea60' : ['LIDAR'],    # Silicon Labs CP210x UART Bridge, klasifikovan kao LIDAR
    '067b:2303' : ['Debug UART', 'IMU'],  # Prolific PL2303, može biti Debug UART ili IMU
    '0403:6001' : ['Debug UART', 'IMU'],  # FT232 UART, može biti Debug UART ili IMU
}

class USB_Mapper:
    """Klasa koja mapira povezane USB uređaje po klasama (Arduino, LIDAR, IMU, ...)"""

    def __init__(self):
        self.table = {}  # rečnik u kojem će ključevi biti klase uređaja, a vrednosti liste device fajlova

        # Šabloni za pretragu USB uređaja
        patterns = [
            '/dev/ttyUSB*',  # standardni USB-Serial uređaji
            '/dev/ttyACM*',  # Arduino i slični USB uređaji koji koriste ACM
        ]
        # pronalazi sve fajlove koji odgovaraju šablonima i spaja ih u jednu listu
        dev_files = sum(
            [glob.glob(p) for p in patterns],
            []
        )

        context = Context()  # kreira udev kontekst za pristup uređajima

        for dev_file in dev_files:  # prolazi kroz svaki pronađeni uređaj
            dev = Device.from_device_file(context, dev_file)  # dobija udev Device objekat

            if False:  # debug opcija, trenutno ugašena
                print(dev_file)
                for k, v in dev.items():
                    print(k, ' = ', v)
                print()

            # TODO: moguće je dodatno koristiti dev.get('ID_PATH') da preciznije odredimo port
            vid = dev.get('ID_VENDOR_ID')  # Vendor ID USB uređaja
            pid = dev.get('ID_MODEL_ID')   # Product ID USB uređaja
            vid_pid = f'{vid}:{pid}'       # spaja VID i PID u string

            # proverava da li je uređaj u definisanoj mapi
            if vid_pid in vid_pid__2_class__map:
                list_of_classes = vid_pid__2_class__map[vid_pid]  # dobija sve klase za ovaj VID:PID
                for c in list_of_classes:
                    if not c in self.table:  # ako klasa još nije u tabeli, dodaje je
                        self.table[c] = []
                    self.table[c].append(dev_file)  # dodaje device fajl u listu klase
            else:
                # ako uređaj nije definisan u mapi, baci grešku
                raise RuntimeError(
                    f'There is no {vid_pid} in vid_pid__2_class__map!'
                )

    def get_exactly_1_dev_of_class(self, class_name):
        """Vraća tačno jedan uređaj iz date klase, ili baci grešku ako ih nema ili ih ima više"""
        c = self.table[class_name]  # uzima listu uređaja za klasu
        if len(c) != 1:
            if len(c) == 0:
                raise RuntimeError(f'{class_name} not connected!')  # nema uređaja
            else:
                raise RuntimeError(f'More than 1 {class_name} connected!')  # više uređaja
        else:
            return c[0]  # vraća jedini uređaj
