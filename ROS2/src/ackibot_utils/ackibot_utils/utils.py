# -*- coding: utf-8 -*-
# Ovaj fajl sadrži pomoćne funkcije i klase za rad sa konzolom, fajlovima i direktorijumima
# Funkcionalnosti uključuju:
# - Ispis vrednosti promenljivih sa njihovim imenima (funkcija show)
# - Ispis poruka različitih nivoa (verbose, debug, info, warn, error, fatal)
# - Rad sa fajl sistemom: provera postojanja fajlova, kreiranje direktorijuma
# - Rekurzivno traženje fajlova/direktorijuma po šablonu
# Sve funkcije su dizajnirane da olakšaju debugging i upravljanje fajlovima u Python projektima

'''
'''

###############################################################################

from __future__ import print_function  # omogućava korišćenje print() funkcije iz Python 3 u Python 2

__author__	    = 'Milos Subotic'  # autor fajla
__email__	    = 'milos.subotic.sm@gmail.com'  # email autora
__copyright__   = 'MIT'  # licenca

###############################################################################

import os  # modul za rad sa fajlovima i direktorijumima
import sys  # modul za interakciju sa sistemom i izlazne poruke
import glob  # modul za pretragu fajlova po šablonu
import re  # modul za regularne izraze
import inspect  # modul za introspektivni rad sa kodom, npr. dohvat linija koda
import errno  # modul za kodove grešaka u OS operacijama
import fnmatch  # modul za šablonsko poređenje fajlova

###############################################################################

# Regularni izraz za pronalaženje izraza show(var) u kodu
_showRegex = re.compile(r'\bshow\s*\(\s*(.*)\s*\)')

def show(var):
    # Funkcija ispisuje ime promenljive i njenu vrednost
    varName = ''  # inicijalizuje ime promenljive praznim stringom
    for line in inspect.getframeinfo(inspect.currentframe().f_back)[3]:  # uzima linije koda poziva funkcije
        m = _showRegex.search(line)  # traži liniju koja koristi show(...)
        if m:
            varName = m.group(1)  # uzima ime promenljive iz regex-a
            break
    print('{0} = {1}'.format(varName, var))  # ispisuje "ime_promenne = vrednost"

###############################################################################

# Nivoi poruka za ispis
VERB  = 0  # verbose
DEBUG = 1  # debug
INFO  = 2  # info
WARN  = 3  # warning
ERROR = 4  # error
FATAL = 5  # fatal (prekida program)

__MSG_PRINT_TYPE = True  # globalna promenljiva koja kontroliše da li se tip poruke ispisuje

def msg_print_type(tf):
    # Funkcija menja __MSG_PRINT_TYPE
    global __MSG_PRINT_TYPE
    __MSG_PRINT_TYPE = tf

def msg(msg_type, *args, **kwargs):
    # Funkcija ispisuje poruke u boji u terminalu prema tipu poruke
    global __MSG_PRINT_TYPE
    if msg_type == VERB:
        color = "\x1b[37m"  # bela boja
        msg_type_str = "verbose"
    elif msg_type == DEBUG:
        color = "\x1b[92m"  # svetlo zelena
        msg_type_str = "debug"
    elif msg_type == INFO:
        color = "\x1b[94m"  # plava
        msg_type_str = "info"
    elif msg_type == WARN:
        color = "\x1b[93m"  # žuta
        msg_type_str = "warning"
    elif msg_type == ERROR:
        color = "\x1b[91m"  # crvena
        msg_type_str = "error"
    elif msg_type == FATAL:
        color = "\x1b[91m"  # crvena
        msg_type_str = "fatal"
    else:
        raise AssertError("Wrong msg_type!")  # ako je tip poruke pogrešan, baca grešku
    
    if __MSG_PRINT_TYPE:
        m = msg_type_str + ":"  # dodaje tip poruke ako je uključen
    else:
        m = ""
    
    print(color + m, sep = '', end = '')  # ispisuje tip poruke u boji
    print(*args, **kwargs, sep = '', end = '')  # ispisuje stvarnu poruku
    print("\x1b[0m", sep = '', end = '') # vraća boju na normalnu
    print()

    if msg_type == FATAL:
        sys.exit(1)  # prekida program ako je fatal poruka

def warn(*args, **kwargs):
    # Funkcija ispisuje warning na stderr
    print('WARN: ', *args, file = sys.stderr, **kwargs)

def error(*args, **kwargs):
    # Funkcija ispisuje error na stderr i prekida program
    print('ERROR: ', *args, file = sys.stderr, **kwargs)
    sys.exit(1)

###############################################################################

def correct_path(path):
    # Funkcija menja '\' u '/' da bi putanje bile kompatibilne sa POSIX sistemom
    return path.replace('\\', '/')
    
def file_exists(path):
    # Funkcija proverava da li fajl postoji
    return os.path.isfile(path)

def mkpath(path):
    # Funkcija kreira direktorijum i sve roditeljske direktorijume (mkdir -p)
    try:
        os.makedirs(path)  # kreira direktorijum
    except OSError as exc:  # ako dođe do greške
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass  # ako direktorijum već postoji, ignoriši grešku
        else:
            raise  # inače ponovo baci grešku

def recursive_glob(pattern, directory = '.'):
    # Rekurzivno traži fajlove i direktorijume koji odgovaraju šablonu
    found = []  # lista za rezultate
    for root, dirs, files in os.walk(str(directory), followlinks = True):  # prolazi kroz sve foldere i fajlove
        dirs = map(lambda s: s + '/', dirs)  # dodaje '/' na krajeve direktorijuma
        for base_name in files:  # prolazi kroz fajlove
            if fnmatch.fnmatch(base_name, pattern):  # proverava da li fajl odgovara šablonu
                found.append(os.path.join(root, base_name))  # dodaje u listu
        for base_name in dirs:  # prolazi kroz foldere
            if fnmatch.fnmatch(base_name, pattern):  # proverava šablon
                found.append(os.path.join(root, base_name))  # dodaje u listu			
    return found  # vraća sve pronađene fajlove/direktorijume

###############################################################################
