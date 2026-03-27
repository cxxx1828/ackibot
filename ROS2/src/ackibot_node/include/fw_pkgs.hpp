
#pragma once

#include "type_shorts.h"
#include "CRC16.hpp"

#define PKG_MAGIC 0xbeef


#define MODULUS 2048


typedef u16 pkg_magic_t;
typedef u16 pkg_crc_t;

#define struct_packed struct __attribute__((packed))

// Paket od pc prema mikrokontroleru
struct_packed pkg_m2s_t {
	pkg_magic_t magic;
	struct_packed {
		i16 speed;
		i16 steering_angle;
		//u16 ramp_rate_ms; // moze se obrisati, ne treba nam
	} payload;
	pkg_crc_t crc;
};

// povratni paket, od mikrokontrolera prema pc-u 
struct_packed pkg_s2m_t {
	pkg_magic_t magic;
	struct_packed {
		//i32 enc[2];
		// treba nam jedan enkoder
		i32 enc;
		// i16 speed_i[2];
		// i16 speed_o[2];
		// kako je ovo povratni paket ne trebaju nam brzine, vec sa mikrokontrolera vracamo rezltat ultrazvucnih senzora
		// to moze biti niz i16, KOLIKA VELICINA????
		i16 speed_i;
		i16 speed_o;
		i16 steering_angle_i;
		i16 steering_angle_o;
		//u8 cfg;
	} payload;
	pkg_crc_t crc;
};


#if !__AVR__ && 0

#include <iterator>
#include <iostream>
#include <iomanip>

static std::ostream& operator<<(std::ostream& os, const pkg_m2s_t& p) {
	std::ios old_state(nullptr);
	old_state.copyfmt(os);
	os
		<< "pkg_m2s_t{"
		<< "speed[0] = " << (int)p.payload.speed[0] << ", "
		<< "speed[1] = " << (int)p.payload.speed[1] << ", "
		<< "ramp_rate_ms = " << (int)p.payload.ramp_rate_ms
		<< "}";
	os.copyfmt(old_state);
	return os;
}

static std::ostream& operator<<(std::ostream& os, const pkg_s2m_t& p) {
	std::ios old_state(nullptr);
	old_state.copyfmt(os);
	os
		<< "pkg_m2s_t{"
		<< "enc[0] = " << (int)p.payload.enc[0] << ", "
		<< "enc[1] = " << (int)p.payload.enc[1] << ", "
		<< "speed_i[0] = " << (int)p.payload.speed_i[0] << ", "
		<< "speed_i[1] = " << (int)p.payload.speed_i[1] << ", "
		<< "speed_o[0] = " << (int)p.payload.speed_o[0] << ", "
		<< "speed_o[1] = " << (int)p.payload.speed_o[1] << ", "
		<< "cfg = " << (int)p.payload.cfg
		<< "}";
	os.copyfmt(old_state);
	return os;
}

#endif
