#!/usr/bin/env python3

import argparse
import time
import subprocess
import os

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

	def get_all_paired_devices():
		out = run_btctl('paired-devices')
		devices = {}
		for line in out.splitlines():
			if line.startswith('Device '):
				parts = line.split(' ', 2)
				if len(parts) == 3:
					mac, name = parts[1], parts[2]
					devices[mac] = name
		return devices
	def get_all_paired_devices_btdevice():
		out = subprocess.run(['bt-device', '-l'], text=True, capture_output=True).stdout
		devices = {}
		for line in out.splitlines():
			if '(' in line and ')' in line:
				name = line.split(' (')[0].strip()
				mac = line.split('(')[-1].split(')')[0]
				devices[mac] = name
		return devices

	def remove_existing_joypads():
		if verbose:
			print('[INFO] Checking for previously paired joypads...')
		paired = get_all_paired_devices_btdevice()
		for mac, name in paired.items():
			for target in TARGET_NAMES:
				if target.lower() in name.lower():
					print(f'[INFO] Removing existing joypad: {name} ({mac})')
					run_btctl(f'disconnect {mac}')
					run_btctl(f'remove {mac}')

	
	run_btctl('power on')
	remove_existing_joypads()
	return True
	
if __name__ == '__main__':
	parser = argparse.ArgumentParser(
		description='Disconnect and remove bluetooth joypad'
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
