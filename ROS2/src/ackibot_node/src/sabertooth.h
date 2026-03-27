
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/**
 * Computes a 7-bit CRC.
 * \param data   The data to compute the CRC of.
 * \param length The length of the data.
 * \return       The CRC.
 */
uint8_t crc7(
	const uint8_t* data,
	uint8_t length
);

/**
 * Computes a 14-bit CRC.
 * \param data   The data to compute the CRC of.
 * \param length The length of the data.
 * \return The CRC.
 */
uint16_t crc14(
	const uint8_t* data,
	uint8_t length
);

/**
 * Writes a CRC-protected Packet Serial command into a buffer.
 * \param address The address of the Sabertooth. By default, this is 128.
 * \param command The command number.
 * \param value   The command value.
 * \param data    Extra data.
 * \param length  The number of bytes of extra data.
 * \param buffer  The buffer to write into.
 * \return How many bytes were written. 
 * 	This always equals 6 + length,
 * 	unless length is 0, in which case it equals 4.
 */
uint8_t writeCRCSabertoothCommand(
	uint8_t address,
	uint8_t command,
	uint8_t value,
	const uint8_t* data,
	uint8_t length,
	uint8_t* buffer
);

/**
 * Writes a CRC-protected Set command into a buffer.
 * \param address The address of the Sabertooth. By default, this is 128.
 * \param setType 0 to set the value, 16 to send a keep-alive,
 * 	32 to set the shutdown state, or 64 to set the serial timeout.
 * \param targetType 'M' for a motor output, 'P' for a power output, etc.
 * \param targetNumber 1 or 2, or a Simplified Serial character like '1' or '2'.
 * \param value The value to set to.
 * \return How many bytes were written. This always equals 10. 
 */
uint8_t writeCRCSabertoothSetCommand(
	uint8_t address,
	uint8_t setType,
	uint8_t targetType,
	uint8_t targetNumber,
	int16_t value,
	uint8_t* buffer
);

#ifdef __cplusplus
} // extern "C"
#endif