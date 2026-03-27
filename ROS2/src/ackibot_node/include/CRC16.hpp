#pragma once

#include <stdint.h>

// 0xa001 is the reversed polynomial for CRC-16/ARC (0x8005)
#define CRC16_DEFAULT_POLYNOME 0xa001
#define CRC16_POLYNOME CRC16_DEFAULT_POLYNOME

// CRC-16/ARC (also known as CRC-16/IBM)
class CRC16 {
public:
	CRC16() {
		restart();
	}
	
	void restart() {
		_crc = 0; // Or 0xffff for CRC-16/CCITT
	}

	CRC16& add(uint8_t value) {
		_crc ^= value;
		for(uint8_t i = 8; i; i--){
			if(_crc & 1){
				_crc = (_crc >> 1) ^ CRC16_POLYNOME;
			}else{
				_crc = _crc >> 1;
			}
		}
		return *this;
	}
	
	CRC16& add(const uint8_t* array, uint16_t length) {
		while(length--){
			add(*array++);
		}
		return *this;
	}
	
	template<typename T>
	CRC16& add(const T& t) {
		add((uint8_t*)&t, sizeof(T));
		return *this;
	}

	uint16_t get_crc() const {
		return _crc;
	}

private:
	uint16_t  _crc;
};

