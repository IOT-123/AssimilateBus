/*
assimilate_bus.h - Library for i2c communication for assimilate iot network (via esp8266). 
This conforming to Crouton (visualization) data structs but can be modified for raw data easily.
Created by Nic Roche, May 23, 2018. 
Released into the public domain.
*/

#ifndef assimilate_bus_h
#define assimilate_bus_h

#include "Arduino.h"
#include "types.h"

typedef void(*NameValueCallback) (char name[16], char value[16], byte address, Role role, PropViz PropVizs);
typedef void(*SlavesProcessedCallback)();

class AssimilateBus
{
public:
	enum PropertyOfInterest {
		ASSIM_NAME,
		ASSIM_ROLE,
		CLOCK_STRETCH,
		VIZ_CARD_TYPE,
		VIZ_ICONS,
		VIZ_LABELS,
		VIZ_MIN,
		VIZ_MAX,
		VIZ_UNITS,
		VIZ_TOTAL,
		VIZ_VALUES,
		VIZ_IS_SERIES,
		VIZ_M_SERIES,
		VIZ_HIGH,
		VIZ_LOW,
		VIZ_TOTL_UNIT,
		NO_INTEREST
	};

	struct Slave
	{
		byte address;
		char name[16];
		Role role;
		int clock_stretch;
		PropViz prop_vizs[5]; // a slave can have 5 published properties
	};
	
	AssimilateBus();
	void get_metadata();
	void print_metadata_details();
	void get_sensors(NameValueCallback nvCallback, SlavesProcessedCallback spCallback);
	PropertyOfInterest get_meta_of_interest(char packet[16]);
	void set_meta_of_interest(Slave &slave, PropertyOfInterest i2cName, char str[16]);
	void send_to_actor(byte address, byte code, const char *param);

private:
	Slave _assimSlaves[10];
	int _deviceCount = 0;
	void scan_bus_for_slave_addresses();
	void set_defaults_for_card_type(char *card_type, PropViz &prop_viz);
	const int _sclPin = D1;
	const int _sdaPin = D2;
};

#endif

