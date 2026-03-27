# https://raspberrypi.stackexchange.com/a/118473
import io  # modul za rad sa fajlovima i tokovima podataka
import os  # modul za interakciju sa operativnim sistemom
# ovaj kod definiše funkciju is_raspberrypi() koja proverava da li se program izvršava na Raspberry Pi računaru

def is_raspberrypi():  # funkcija koja proverava da li se kod izvršava na Raspberry Pi
    if os.name != 'posix':  # ako OS nije POSIX (Linux/macOS), sigurno nije Raspberry Pi
        return False  # vraća False jer Raspberry Pi radi na Linux-u

    chips = ('BCM2708','BCM2709','BCM2711','BCM2835','BCM2836')  
    # lista poznatih Broadcom čipova koji se koriste na Raspberry Pi modelima

    try:
        with io.open('/proc/cpuinfo', 'r') as cpuinfo:  # otvara fajl /proc/cpuinfo koji sadrži informacije o hardveru
            for line in cpuinfo:  # prolazi kroz svaku liniju fajla
                if line.startswith('Hardware'):  # traži liniju koja počinje sa "Hardware"
                    _, value = line.strip().split(':', 1)  # deli liniju po ':' i uzima vrednost hardvera
                    value = value.strip()  # uklanja nepotrebne razmake
                    if value in chips:  # proverava da li je vrednost u listi poznatih Raspberry Pi čipova
                        return True  # ako jeste, vraća True jer se radi o Raspberry Pi
    except Exception:  # ako dođe do greške (npr. fajl ne postoji)
        pass  # ignoriše grešku i nastavlja

    return False  # ako nije pronađen odgovarajući hardver ili je došlo do greške, vraća False
