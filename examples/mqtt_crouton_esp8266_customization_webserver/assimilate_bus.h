/*
assimilate_bus.h - Library for i2c communication for assimilate iot network (via esp8266). 
This conforming to Crouton (visualization) data structs but can be modified for raw data easily.
Created by Nic Roche, May 23, 2018. 
Released into the public domain.
*/

#ifndef assimilate_bus_h
#define assimilate_bus_h

#include <ArduinoJson.h>
#include "Arduino.h"
#include "types.h"



typedef void(*NameValueCallback) (byte slave_address, byte prop_index, Role role, char name[16], char value[16]);
typedef void(*SlavesProcessedCallback)();

class AssimilateBus
{
public:

  struct Slave
  {
      byte address;
      char name[16];
      Role role;
      int clock_stretch;
    };
	
	AssimilateBus();
	bool get_user_meta_or_default(const JsonArray& json_array, char name[16], char packet[64]);
	void get_metadata();
	void print_metadata_details();
	void get_sensors(NameValueCallback nvCallback, SlavesProcessedCallback spCallback);
	void set_meta_of_interest(Slave &slave, JsonObject &address_json_obj, char i2cName[16], char packet[16]);
	void send_to_actor(byte address, byte code, const char *param);
private:
	Slave _assimSlaves[10]{};
	int _deviceCount = 0;
	void scan_bus_for_slave_addresses();
	void set_defaults_for_card_type(char *card_type, JsonObject &idx_json_obj);
	int I2C_ClearBus();
	byte parse_prop_index(const char* value, int position);
	const int _sclPin = D1;
	const int _sdaPin = D2;
};

#endif

