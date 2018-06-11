/*
assimilate_bus.cpp - Library for i2c communication for assimilate iot network (via esp8266)
Created by Nic Roche, May 23, 2018.
Released into the public domain.
*/
#include <Wire.h>
#include <AssimilateBus.h>

#define PIN_RESET D0

AssimilateBus::Slave _defaultSlave = { 0, "SLAVE_DEFAULTS", ACTOR, 40000, {} };

AssimilateBus::AssimilateBus()
{
	
  int rtn = I2C_ClearBus(); // clear the I2C bus first before calling Wire.begin()
  if (rtn != 0) {
    Serial.println(F("I2C bus error. Could not clear"));
    if (rtn == 1) {
      Serial.println(F("SCL clock line held low"));
    } else if (rtn == 2) {
      Serial.println(F("SCL clock line held low by slave clock stretch"));
    } else if (rtn == 3) {
      Serial.println(F("SDA data line held low"));
    }
  } else { // bus clear
      // soft hardware reset
    pinMode(PIN_RESET, OUTPUT);
    digitalWrite(PIN_RESET, HIGH);
    // re-enable Wire
	Wire.begin(_sdaPin, _sclPin);
	scan_bus_for_slave_addresses();
  }
}

/**
 * This routine turns off the I2C bus and clears it
 * on return SCA and SCL pins are tri-state inputs.
 * You need to call Wire.begin() after this to re-enable I2C
 * This routine does NOT use the Wire library at all.
 *
 * returns 0 if bus cleared
 *         1 if SCL held low.
 *         2 if SDA held low by slave clock stretch for > 2sec
 *         3 if SDA held low after 20 clocks.
 */
int AssimilateBus::I2C_ClearBus() {
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(SCL, INPUT_PULLUP);

  delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.

  boolean SCL_LOW = (digitalRead(SCL) == LOW); // Check is SCL is Low.
  if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(SDA) == LOW);  // vi. Check SDA input.
  int clockCount = 20; // > 2x9 clock

  while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
    clockCount--;
  // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
    pinMode(SCL, INPUT); // release SCL pullup so that when made output it will be LOW
    pinMode(SCL, OUTPUT); // then clock SCL Low
    delayMicroseconds(10); //  for >5uS
    pinMode(SCL, INPUT); // release SCL LOW
    pinMode(SCL, INPUT_PULLUP); // turn on pullup resistors again
    // do not force high as slave may be holding it low for clock stretching.
    delayMicroseconds(10); //  for >5uS
    // The >5uS is so that even the slowest I2C devices are handled.
    SCL_LOW = (digitalRead(SCL) == LOW); // Check if SCL is Low.
    int counter = 20;
    while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
      counter--;
      delay(100);
      SCL_LOW = (digitalRead(SCL) == LOW);
    }
    if (SCL_LOW) { // still low after 2 sec error
      return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
    }
    SDA_LOW = (digitalRead(SDA) == LOW); //   and check SDA input again and loop
  }
  if (SDA_LOW) { // still low
    return 3; // I2C bus error. Could not clear. SDA data line held low
  }

  // else pull SDA line low for Start or Repeated Start
  pinMode(SDA, INPUT); // remove pullup.
  pinMode(SDA, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
  // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
  /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
  delayMicroseconds(10); // wait >5uS
  pinMode(SDA, INPUT); // remove output low
  pinMode(SDA, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
  delayMicroseconds(10); // x. wait >5uS
  pinMode(SDA, INPUT); // and reset pins as tri-state inputs which is the default state on reset
  pinMode(SCL, INPUT);
  return 0; // all ok
}

void AssimilateBus::get_metadata()
{
		char packet[16];
		char name[16];
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
Serial.print("[0]");
Serial.println(packet);
						strcpy(name, packet);
						// find if property of interest and tag
						i2cName = AssimilateBus::get_meta_of_interest(packet);
						break;
					case 1:
						// set in property of interest
Serial.print("[1]");
Serial.println(packet);
						if (strcmp(name, packet)==0){// this is the main symptom of message sequence is out of sync
							Serial.println("OUT OF SYNC");
							Serial.println("RESTARTING");
							i2cNodeProcessed = true;
							digitalWrite(PIN_RESET, LOW); //ToDo: the slaves need to be rebooted
							break;
						}
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
// Serial.println("get_sensors");
// Serial.println(segment);
// Serial.println(packet);
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
						// if "prepare" msg goto next - will only happen on loop interval
						// char compare_str[7];
						// strncpy(compare_str, name, 7);
						// if (strcmp("prepare", strlwr(compare_str)) == 0){
						// 	Serial.println("-----------PREPARE ONLY!");
						// 	i2cNodeProcessed = true;
						// 	break;
						// }
						// bubble up the name / value pairs
						nvCallback(name, value, _assimSlaves[index].address, _assimSlaves[index].role, _assimSlaves[index].prop_vizs[property_index]);
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
	if (str.equals("VIZ_IS_SERIES"))
	{
		return AssimilateBus::VIZ_IS_SERIES;
	}
	if (str.equals("VIZ_M_SERIES"))// MULTI SERIES - MULTIPLE LINES ON LINE CHASRT
	{
		return AssimilateBus::VIZ_M_SERIES;
	}
	if (str.equals("VIZ_HIGH"))
	{
		return AssimilateBus::VIZ_HIGH;
	}
	if (str.equals("VIZ_LOW"))
	{
		return AssimilateBus::VIZ_LOW;
	}
	if (str.equals("VIZ_TOTL_UNIT"))
	{
		return AssimilateBus::VIZ_TOTL_UNIT;
	}
	return AssimilateBus::NO_INTEREST;
}

void AssimilateBus::set_defaults_for_card_type(char *card_type, PropViz &prop_viz){
//Serial.println("set_defaults_for_card_type");
	if (strcmp("toggle", card_type) == 0)
	{
//Serial.println("set_defaults_for_card_type toggle");

	}
	if (strcmp("input", card_type) == 0)
	{

	}
	if (strcmp("text", card_type) == 0)
	{
		strcpy(prop_viz.units, "STATUS");
		return;
	}
	if (strcmp("slider", card_type) == 0)
	{

	}
	if (strcmp("button", card_type) == 0)
	{

	}
	if (strcmp("chart-donut", card_type) == 0)
	{

		prop_viz.total = 100;
		strcpy(prop_viz.units, "%");
		strcpy(prop_viz.values[0].name, "labels");
		strcpy(prop_viz.values[0].value, "[]");
		strcpy(prop_viz.values[1].name, "series");
		strcpy(prop_viz.values[1].value, "0");
		return;
	}
	if (strcmp("chart-line", card_type) == 0)
	{
		prop_viz.max = 12;
		prop_viz.low = 0;
		prop_viz.high = 100;
	}

}


void AssimilateBus::set_meta_of_interest(AssimilateBus::Slave &slave, AssimilateBus::PropertyOfInterest i2cName, char packet[16])
{
	int idx = -1;
	char *pair;
	char *name;
	char *units;
	char *value;
	switch (i2cName) {
	case AssimilateBus::ASSIM_NAME:
		strcpy(slave.name, packet);
		break;
	case AssimilateBus::ASSIM_ROLE:
		slave.role = (strcmp(packet, "SENSOR") == 0) ? SENSOR : ACTOR;
		break;
	case AssimilateBus::CLOCK_STRETCH:
		slave.clock_stretch = atoi(packet);
		break;
	case AssimilateBus::VIZ_CARD_TYPE:  // "0:toggle" idx:value
		idx = atoi(strtok(packet, ":|"));
		value = strtok(NULL, ":|");
		strcpy(slave.prop_vizs[idx].card_type, value);
		set_defaults_for_card_type(value, slave.prop_vizs[idx]);
		set_defaults_for_card_type("chart-line", slave.prop_vizs[idx]);
		break;
	case AssimilateBus::VIZ_ICONS: // "0:0|lightbulb" idx:name|value
		// break up idx, name, value
		idx = atoi(strtok(packet, ":|"));
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
		idx = atoi(strtok(packet, ":|"));
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
		idx = atoi(strtok(packet, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].max = atoi(value);
		break;
	case AssimilateBus::VIZ_MIN: // idx:value
		// break up idx, value
		idx = atoi(strtok(packet, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].min = atoi(value);
		break;
	case AssimilateBus::VIZ_TOTAL: // idx:value
		// break up idx, value
		idx = atoi(strtok(packet, ":|"));
		value = strtok(NULL, ":|");
		slave.prop_vizs[idx].total = atoi(value);
		break;
	case AssimilateBus::VIZ_UNITS: // idx:value
		// break up idx, value
		idx = atoi(strtok(packet, ":|"));
		value = strtok(NULL, ":|");
		strcpy(slave.prop_vizs[idx].units, value);
		break;
	case AssimilateBus::VIZ_VALUES: //"0:value|0" idx:name|value
									// break up idx, name, value
		idx = atoi(strtok(packet, ":|"));
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
	case AssimilateBus::VIZ_IS_SERIES: // "2:true"  idx:value 
		// break up idx, value
		idx = atoi(strtok(packet, ":"));
		value = strtok(NULL, ":");
		slave.prop_vizs[idx].is_series = strcmp(value, "true")==0;
		break;
	case AssimilateBus::VIZ_HIGH: // idx:value
		// break up idx, value
		idx = atoi(strtok(packet, ":"));
		value = strtok(NULL, ":");
		slave.prop_vizs[idx].high = atoi(value);
		break;
	case AssimilateBus::VIZ_LOW: // idx:value
		// break up idx, value
		idx = atoi(strtok(packet, ":"));
		value = strtok(NULL, ":");
		slave.prop_vizs[idx].low = atoi(value);
		break;
	case AssimilateBus::VIZ_TOTL_UNIT: // "0:100|%" idx:total|unit
		// break up idx, total, unit
		idx = atoi(strtok(packet, ":|"));
		slave.prop_vizs[idx].total = atoi(strtok(NULL, ":|"));
		units = strtok(NULL, ":|");
		strcpy(slave.prop_vizs[idx].units, units);
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
