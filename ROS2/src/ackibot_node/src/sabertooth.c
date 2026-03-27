
#include "sabertooth.h"

//Na osnovu komandi, sabertooth sam brine o stvarima kao što su:
//PWM generacija,
//kontrola struje,
//zaštita od preopterećenja i pregrevanja,
//meko zaustavljanje/start.
//umesto da mi generisemo PWM signal Sabertooth to radi za nas


//omogucava da se posalju sigurne komande Sabertooth motor kontroleru
uint8_t crc7(
	const uint8_t* data,				//prima pokazivac na niz bajtova i duzinu
	uint8_t length
) {
	uint8_t crc = 0x7f;					//01111111
	uint8_t i, bit;
	for(i = 0; i < length; i++){
		crc ^= data[i];				//crc se xoruje sa svakim bajtom
		for(bit = 0; bit < 8; bit++){
			if(crc & 1){
				crc >>= 1;		//ako je najmanje znacajan bit 1, crc se pomera udesno i xoruje sa 0x76
				crc ^= 0x76;	//0x76 je generatorski polinom 
			}else{
				crc >>= 1;
			}
		}
	}
	return crc ^ 0x7f;			//kada su svi bajtovi obradjeni, crc se xoruje sa 0x7f
}
//CRC7 je kod za detekciju gresaka koji koristi 7 bita za proveru integriteta podataka.
//omogucava detekciju gresaka u komunikaciji

//slicno kao prethodno, koristi veci polinom i vise bitova
uint16_t crc14(
	const uint8_t* data,
	uint8_t length
) {
	uint16_t crc = 0x3fff;			//0011111111111111
	uint8_t i, bit;
	for(i = 0; i < length; i ++){
		crc ^= data[i];
		for(bit = 0; bit < 8; bit ++){
			if(crc & 1){
				crc >>= 1;
				crc ^= 0x22f0;
			}else{
				crc >>= 1;
			}
		}
	}
	return crc ^ 0x3fff;
}

uint8_t writeCRCSabertoothCommand(
	uint8_t address,
	uint8_t command,
	uint8_t value,
	const uint8_t* data,
	uint8_t length,
	uint8_t* buffer
) {
	uint8_t i;
	uint16_t crc;
	buffer[0] = address + 112;			//sabertooth zahteva da se adresa poveca za 112
	buffer[1] = command;			//komanda
	buffer[2] = value;			//vrednost komande
	buffer[3] = crc7(buffer, 3);		//uzimaju se prva tri bajta poruke i racuna se crc7
	if(length == 0){
		return 4;						//ako nema dodatnih podataka, poruka je gotova, vraca se duzina 4 jer ima 4 bajta
	}else{
		for(i = 0; i < length; i ++){
			buffer[4 + i] = data[i];		//ako ima dodatnih podataka, oni se kopiraju u poruku
		}									//na pozicije posle prva 4 bajta jer su oni popunjeni sa adresa, komanda, vrednost i crc7
		//racuna se crc14 od 5. bita do kraja poruke
		crc = crc14(buffer + 4, length);
		buffer[4 + length] = (uint8_t)((crc >> 0) & 127);
		buffer[5 + length] = (uint8_t)((crc >> 7) & 127);
		return 6 + length;
	}
}

//funkcija koja pravi komandu za postavljanje vrednosti
//spakuje sve u jedan bafer spreman za slanje Sabertooth-u

uint8_t writeCRCSabertoothSetCommand(
	uint8_t address,
	uint8_t setType,
	uint8_t targetType,
	uint8_t targetNumber,
	int16_t value,
	uint8_t* buffer
) {
	uint8_t data[4];
	data[2] = targetType;
	data[3] = targetNumber;
	//ako je vrednost negativna, postavlja se znak i vrednost se pretvara u pozitivnu
	if(value < 0){
		value = -value;
		data[0] = (uint8_t)((value >> 0) & 127);
		data[1] = (uint8_t)((value >> 7) & 127);
		return writeCRCSabertoothCommand(
			address,
			40,
			setType + 1,
			data,
			4,
			buffer
		);
	}else{
		data[0] = (uint8_t)((value >> 0) & 127);
		data[1] = (uint8_t)((value >> 7) & 127);
		return writeCRCSabertoothCommand(
			address,
			40,
			setType,
			data,
			4,
			buffer
		);
	}
}