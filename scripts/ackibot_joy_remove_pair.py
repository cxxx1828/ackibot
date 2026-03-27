#!/usr/bin/env python3

import argparse
import time
import subprocess
import os
import pathlib

# Config
TARGET_NAMES = ['Wireless Controller', 'DUALSHOCK', 'Sony', 'Bluetooth Gamepad']
SCAN_DURATION = 3 # seconds
SCAN_TRIES = 100
BLUETOOTHCTL = '/usr/bin/bluetoothctl'


def main(
	verbose
):
	btctl_process = subprocess.Popen(
		[BLUETOOTHCTL],
		stdin=subprocess.PIPE,
		stdout=subprocess.PIPE,
		stderr=subprocess.DEVNULL,
		text=True
	)
	os.set_blocking(btctl_process.stdout.fileno(), False)

	def run_btctl(cmd, delay = 0.1):
		def read_from_btctl():
			out = ''
			while True:
				line = btctl_process.stdout.readline()
				if not line:
					break
				else:
					out += line
				if verbose:
					print(line, end = '')
			return out

		read_from_btctl()
		#if verbose:
		#	print('[bluetooth]# ' + cmd)
		btctl_process.stdin.write(cmd + '\n')
		btctl_process.stdin.flush()
		time.sleep(delay)
		return read_from_btctl()

	def get_nearby_devices():
		if verbose:
			print('[INFO] Scanning for nearby Bluetooth devices...')
		run_btctl('scan on')
		time.sleep(SCAN_DURATION)
		out = run_btctl('devices')
		#run_btctl('scan off')
		devices = {}
		for line in out.splitlines():
			if line.startswith('Device '):
				parts = line.split(' ', 2)
				if len(parts) == 3:
					mac, name = parts[1], parts[2]
					for target in TARGET_NAMES:
						if target.lower() in name.lower():
							devices[mac] = name
		return devices

	def connect_device(mac, name):
		print(f'[INFO] Attempting to pair/trust/connect to {name} ({mac})...')

		time.sleep(1)
		run_btctl(f'pair {mac}')
		run_btctl(f'trust {mac}')
		time.sleep(1)
		out = run_btctl(f'connect {mac}', 5)

		if verbose:
			print(f'[INFO] Done attempting connection to {name}.')

		lines = out.split('\n')
		connected = any(line.endswith('Connection successful') for line in lines)
		if connected:
			print(f'[INFO] Connection successful to {name} :)')

		return connected


	def scan():
		# Try a lot of times.
		for i in range(SCAN_TRIES):
			devices = get_nearby_devices()
			if not devices:
				if verbose:
					print(f'[WARN] No joypads found in scan {i}/{SCAN_TRIES}.')
			else:
				return devices
		print('[ERROR] After long time of scanning cannot find joypad :(')
		return None

	def connect(devices):
		if len(devices) != 1:
			print(f'[WARN] Strange number of joypads.')
		for mac, name in devices.items():
			connected = connect_device(mac, name)
			if connected:
				return True
		return False
	

	script_dir = pathlib.Path(__file__).parent.resolve()
	subprocess.run([os.path.join(script_dir, 'ackibot_joy_remove.py')])
	#run_btctl('power on')

	
	print('[INFO] Please press button(s) on your bluetooth joypad for PAIRING...')

	# Try a lot of times.
	for i in range(10):
		devices = scan()
		if not devices:
			return False
		else:
			connected = connect(devices)
			if connected:
				return True

	print('[ERROR] After long time trying cannot connect to joypad :(')
	return False
	
if __name__ == '__main__':
	parser = argparse.ArgumentParser(
		description='Pair with bluetooth joypad'
	)
	parser.add_argument(
		'-v',
		'--verbose',
		action='store_true',
		help='Enable verbose output.'
	)

	args = parser.parse_args()

	main(
		verbose = args.verbose
	)
