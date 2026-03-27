import serial, struct

def crc16_arc(data: bytes) -> int:
    crc = 0x0000
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

# pkg_m2s_t: magic (u16) + payload (i16 speed, i16 angle, u16 ramp_ms) + crc (u16)
magic = 0xBEEF
speed = 500
angle = 120
ramp_ms = 0

payload = struct.pack('<hhi', speed, angle, ramp_ms)[:6]  # < = little-endian
# gore koristimo '<' jer je AVR little-endian, a kod na Arduinu radi reinterpret_cast kao u memoriji

p_wo_crc = struct.pack('<H', magic) + payload
crc = crc16_arc(payload)  # na Arduinu se CRC računa samo nad payload-om: CRC16().add(p.payload)

packet = p_wo_crc + struct.pack('<H', crc)

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.2) as ser:
    ser.write(packet)
    # očekujemo odgovor pkg_s2m_t (binarno); za brzu provjeru, čitaj samo par bajtova:
    resp = ser.read(32)
    print("RX bytes:", resp)

