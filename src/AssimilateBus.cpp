/*
assimilate_bus.cpp - Library for i2c communication for assimilate iot network (via esp8266)
Created by Nic Roche, May 23, 2018.
Released into the public domain.
*/
#include <Wire.h>
#include <AssimilateBus.h>

AssimilateBus::Slave _defaultSlave = { 0, "SLAVE_DEFAULTS", ACTOR, 40000, {} };

AssimilateBus::AssimilateBus()
{
	Wire.begin(_sdaPin, _sclPin);
	scan_bus_for_slave_addresses();
}

void AssimilateBus::get_metadata()
{
		char packet[16];
		bool i2cNodeProcessed;
		int intOfChar;
		PropertyOfInterest i2cName;
		for (byte index = 0; index < _deviceCount; index++)
		{
			byte slaveAddress = _assimSlaves[index].address;
			while (true)
			{
				i2cNodeProcessed = false;
				for (byte segment = 0; segment < 3; segment++)
				{
					Wire.requestFrom(slaveAddress, 16);
					byte writeIndex = 0;
					while (Wire.available()) { // slave may send less than requested
						const char c = Wire.read();
						intOfChar = int(c);
						packet[writeIndex] = intOfChar > -1 && intOfChar < 255 ? c : 0x00;// replace invalid chars with zero char      
						writeIndex++;
					}// end while wire.available
					switch (segment)
					{
					case 0:
//Serial.print("[0]");
//Serial.println(packet);
						// find if property of interest and tag
						i2cName = AssimilateBus::get_meta_of_interest(packet);
						break;
					case 1:
						// set in property of interest
//Serial.print("[1]");
//Serial.println(packet);
						AssimilateBus::set_meta_of_interest(_assimSlaves[index], i2cName, packet);
						break;
					case 2:
						// check if last metadata
						if (int(packet[0]) != 49) {// 0 on last property
							i2cNodeProcessed = true;
							break;
						}
					default:;
					}
				}// end for segment  
				if (i2cNodeProcessed) { // break out if last metadata
					break;
				}
			}// end while true
			 // send metaconfirmed
			Serial.print("------------------------CONFIRMING METADATA FOR ");
			Serial.println(_assimSlaves[index].name);
			delay(1);
			Wire.beginTransmission(slaveAddress);
			Wire.write(1);
			Wire.endTransmission();
			delay(100);
		}// end for index	
}

void AssimilateBus::get_sensors(NameValueCallback nvCallback, SlavesProcessedCallback spCallback)
{
	char packet[16];
	char name[16];
	char value[16];
	bool i2cNodeProcessed;
	int intOfChar;
	PropertyOfInterest i2cName;
	for (byte index = 0; index < _deviceCount; index++)
	{
		byte property_index = 0;
		byte slaveAddress = _assimSlaves[index].address;
		while (true)
		{
			i2cNodeProcessed = false;
			for (byte segment = 0; segment < 3; segment++)
			{
				Wire.setClockStretchLimit(_assimSlaves[index].clock_stretch);
				Wire.requestFrom(slaveAddress, 16);
				byte writeIndex = 0;
				while (Wire.available()) { // slave may send less than requested
					const char c = Wire.read();
					intOfChar = int(c);
					packet[writeIndex] = intOfChar > -1 && intOfChar < 255 ? c : 0x00;// replace invalid chars with zero char      
					writeIndex++;
				}// end while wire.available
				switch (segment)
				{
				case 0:
					// set property name
					strcpy(name, packet);
					break;
				case 1:
					// set property value
					strcpy(value, packet);
					break;
				case 2:
					// bubble up the name / value pairs
					nvCallback(strlwr(name), value, _assimSlaves[index].address, _assimSlaves[index].role, _assimSlaves[index].prop_vizs[property_index]);
					property_index++;
					// check if last metadata
					if (int(packet[0]) != 49) {// 0 on last property
						i2cNodeProcessed = true;
						break;
					}
				default:;
				}
			}// end for segment
			if (i2cNodeProcessed) { // break out of true loop if last metadata
				break;
			}
		}// end while true
	}// end for index	
	spCallback();
}

void AssimilateBus::print_metadata_details()
{
	Serial.print("DEVICE COUNT: ");
	Serial.println(_deviceCount);
	for (byte index = 0; index < _deviceCount; index++)
	{
		Serial.print(_assimSlaves[index].address);
		Serial.print("\t");
		Serial.print(_assimSlaves[index].name);
		Serial.print("\t");
		Serial.print(_assimSlaves[index].role == ACTOR ? "ACTOR" : "SENSOR");
		Serial.print("\t");
		Serial.print(_assimSlaves[index].clock_stretch);
		Serial.println();
	}	
}

void AssimilateBus::scan_bus_for_slave_addresses()
{
	_deviceCount = 0;
	for (byte address = 8; address < 127; address++)
	{
		Wire.beginTransmission(address);
		const byte error = Wire.endTransmission();
		if (error == 0)
		{
			_assimSlaves[_deviceCount] = _defaultSlave;
			_assimSlaves[_deviceCount].address = address;
			_deviceCount++;
		}
	}	
}

AssimilateBus::PropertyOfInterest AssimilateBus::get_meta_of_interest(char packet[16])
{
	// search opperations so expensive string used - metadata only
	String str(packet);
	if (str.equals("ASSIM_NAME"))
	{
		return AssimilateBus::ASSIM_NAME;
	}
	if (str.equals("ASSIM_ROLE"))
	{
		return AssimilateBus::ASSIM_ROLE;
	}
	if (str.equals("CLOCK_STRETCH"))
	{
		return AssimilateBus::CLOCK_STRETCH;
	}
	if (str.equals("VIZ_CARD_TYPE"))
	{
		return AssimilateBus::VIZ_CARD_TYPE;
	}
	if (str.equals("VIZ_ICONS"))
	{
		return AssimilateBus::VIZ_ICONS;
	}
	if (str.equals("VIZ_LABELS"))
	{
		return AssimilateBus::VIZ_LABELS;
	}
	if (str.equals("VIZ_MIN"))
	{
		return AssimilateBus::VIZ_MIN;
	}
	if (str.equals("VIZ_MAX"))
	{
		return AssimilateBus::VIZ_MAX;
	}
	if (str.equals("VIZ_UNITS"))
	{
		return AssimilateBus::VIZ_UNITS;
	}
	if (str.equals("VIZ_TOTAL"))
	{
		return AssimilateBus::VIZ_TOTAL;
	}
	if (str.equals("VIZ_VALUES"))
	{
		return AssimilateBus::VIZ_VALUES;
	}
	return AssimilateBus::NO_INTEREST;
}

void AssimilateBus::set_meta_of_interest(AssimilateBus::Slave &slave, AssimilateBus::PropertyOfInterest i2cName, char str[16])
{
	int idx = -1;
	char *pair;
	char *name;
	char *value;
	switch (i2cName) {
	case AssimilateBus::ASSIM_NAME:
		strcpy(slave.name, str);
		break;
	case AssimilateBus::ASSIM_ROLE:
		slave.role = (strcmp(str, "SENSOR") == 0) ? SENSOR : ACTOR;
		break;
	case AssimilateBus::CLOCK_STRETCH:
		slave.clock_stretch = atoi(str);
		break;
	case AssimilateBus::VIZ_CARD_TYPE:  // "0:toggle" idx:value
		idx = atoi(strtok(str, ":|"));
		value = strtok(NULL, ":|");
		strcpy(slave.prop_vizs[idx].card_type, value);
		break;
	case AssimilateBus::VIZ_ICONS: // "0:0|lightbulb" idx:name|value
		// break up idx, name, value
		idx = atoi(strtok(str, ":|"));
		name = strtok(NULL, ":|");
		value = strtok(NULL, ":|");
		//get unused item
		if (strcmp(slave.prop_vizs[idx].icons[0].name, "") == 0){
			strcpy(slave.prop_vizs[idx].icons[0].name, name);
			strcpy(slave.prop_vizs[idx].icons[0].value, value);
		}else{
			strcpy(slave.prop_vizs[idx].icons[1].name, name);
			strcpy(slave.prop_vizs[idx].icons[1].value, value);
		}
		break;
	case AssimilateBus::VIZ_LABELS: // "0:false|OFF" idx:name|value
		// break up idx, name, value
		idx = atoi(strtok(str, ":|"));
		name = strtok(NULL, ":|");
		value = strtok(NULL, ":|");
		//get unused item
		if (strcmp(slave.prop_vizs[idx].labels[0].name, "") == 0) {
			strcpy(slave.prop_vizs[idx].labels[0].name, name);
			strcpy(slave.prop_vizs[idx].labels[0].value, value);
		}
		else {
			strcpy(slave.prop_vizs[idx].labels[1].name, name);
			strcpy(slave.prop_vizs[idx].labels[1].value, value);
		}		
		break;
	case AssimilateBus::VIZ_MAX: // idx:value
		// break up idx, value
		idx = atoi(strtok(str, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].max = atoi(value);
		break;
	case AssimilateBus::VIZ_MIN: // idx:value
		// break up idx, value
		idx = atoi(strtok(str, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].min = atoi(value);
		break;
	case AssimilateBus::VIZ_TOTAL: // idx:value
		// break up idx, value
		idx = atoi(strtok(str, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].total = atoi(value);
		break;
	case AssimilateBus::VIZ_VALUES: //"0:value|0" idx:name|value
									// break up idx, name, value
		idx = atoi(strtok(str, ":|"));
		name = strtok(NULL, ":|");
		value = strtok(NULL, ":|");
		//get unused item
		if (strcmp(slave.prop_vizs[idx].values[0].name, "") == 0) {
			strcpy(slave.prop_vizs[idx].values[0].name, name);
			strcpy(slave.prop_vizs[idx].values[0].value, value);
		}else if (strcmp(slave.prop_vizs[idx].values[1].name, "") == 0) {
			strcpy(slave.prop_vizs[idx].values[1].name, name);
			strcpy(slave.prop_vizs[idx].values[1].value, value);
		}
		else {
			strcpy(slave.prop_vizs[idx].values[2].name, name);
			strcpy(slave.prop_vizs[idx].values[2].value, value);
		}
		break;
	default:
		// got nothin for ya
		break;
	}
}

void AssimilateBus::send_to_actor(byte address, byte code, const char *param)
{
	Serial.println("send_to_actor");
	Serial.println(address);
	Serial.println(code);
	Serial.println(param);
	delay(1);
	Wire.beginTransmission(address);
	Wire.write(code);
	Wire.write(param);
	Wire.endTransmission();
	delay(100);
}
