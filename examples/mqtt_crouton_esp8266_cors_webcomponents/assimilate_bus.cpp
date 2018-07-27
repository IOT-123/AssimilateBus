/*
assimilate_bus.cpp - Library for i2c communication for assimilate iot network (via esp8266)
Created by Nic Roche, May 23, 2018.
Released into the public domain.
*/
#include <Wire.h>
#include "assimilate_bus.h"
#include "config.h"
#include "debug.h"


#define PIN_RESET D0
#define PIN_GND_SWITCH D7
#define DBG_OUTPUT_FLAG 1  //0,1,2 NONE,RELREASE,FULL

Debug _debug_assim(DBG_OUTPUT_FLAG);
Config _config;

AssimilateBus::Slave _defaultSlave = { 0, "SLAVE_DEFAULTS", ACTOR, 40000 };

AssimilateBus::AssimilateBus(){
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
		// power for slaves
		pinMode(PIN_GND_SWITCH, OUTPUT);
		digitalWrite(PIN_GND_SWITCH, HIGH);
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

void AssimilateBus::get_metadata(){
	_debug_assim.out_fla(F("[METADATA BUILD] LOAD USER CUSTOMIZATIONS"), true, 1);
  // CLEAR FOLDER /config/metadata
  _config.clear_metadata_dir();
  char packet[16];
  bool i2c_node_processed;
  int int_of_char;
  char name[16];
  for (byte index = 0; index < _deviceCount; index++)
  {
    byte slave_address = _assimSlaves[index].address;
	char path[31];
	sprintf(path, _config.FILE_CONFIG_USER_METAS, slave_address);
	String json_read = _config.read(path);
	const size_t buffer_size = (json_read.length() * 2) + 8;
	DynamicJsonBuffer json_read_buffer(buffer_size);
	JsonArray& user_metas_json_array = json_read_buffer.parseArray(json_read);
	DynamicJsonBuffer json_write_buffer;
	JsonObject& address_json_obj = json_write_buffer.createObject();
	_debug_assim.out_fla(F("[METADATA REQUEST] SLAVEID: "), false, 1);
	_debug_assim.out_str(String(slave_address), true, 1);
    while (true)
    {
      i2c_node_processed = false;
      for (byte segment = 0; segment < 3; segment++) { // 3 requests per meta
        Wire.requestFrom(slave_address, 16); 
        byte writeIndex = 0;
        while (Wire.available()) { // slave may send less than requested
          const char c = Wire.read();
          int_of_char = int(c);
          packet[writeIndex] = int_of_char > -1 && int_of_char < 255 ? c : 0x00;// replace invalid chars with zero char      
          writeIndex++;
        }// end while wire.available
        switch (segment)
        {
        case 0:
          strcpy(name, packet);
          _debug_assim.out_char(packet, false, 1);
          _debug_assim.out_fla(F("\t"), false, 1);
          break;
        case 1:
			if (strcmp(name, packet) == 0) {// this is the main symptom of message sequence is out of sync
				_debug_assim.out_fla(F("OUT OF SYNC"), true, 0);
				_debug_assim.out_fla(F("RESTARTING"), true, 0);
				i2c_node_processed = true;
				digitalWrite(PIN_RESET, LOW);
				digitalWrite(PIN_GND_SWITCH, LOW);
				break;
			}
			// set in property of interest
			char packet_or_user_meta[64]; // allow richer values from JSDON files
			strcpy(packet_or_user_meta, packet);
			get_user_meta_or_default(user_metas_json_array, name, packet_or_user_meta);
			_debug_assim.out_char(packet_or_user_meta, true, 1);
			set_meta_of_interest(_assimSlaves[index], address_json_obj, name, packet_or_user_meta);
			break;
        case 2:
          // check if last metadata
          if (int(packet[0]) != 49) {// 0 on last property
            i2c_node_processed = true;
            break;
          }
        default:;
        }
      }// end for segment  
      if (i2c_node_processed) { // break out if last metadata
        break;
      }
    }// end while true
    _debug_assim.out_fla(F("------------------------CONFIRMING METADATA FOR "), false, 1);
    _debug_assim.out_char(_assimSlaves[index].name, true, 1);
    delay(1);
    Wire.beginTransmission(slave_address);
    Wire.write(1);
    Wire.endTransmission();
    delay(100);
    // SAVE /config/metadata/<i>.json
    _config.set_slave_meta_json(slave_address, address_json_obj);
  }// end for index 

}// get_metadata

void AssimilateBus::set_meta_of_interest(Slave &slave, JsonObject &address_json_obj, char i2cName[16], char packet[16]){
  char *name;
  char *units;
  char *value;
  String str(i2cName);
  if (str.equals("ASSIM_NAME"))
  {
    strcpy(slave.name, packet);
    return;
  }
  if (str.equals("ASSIM_VERSION"))
  {
    return;
  }
  if (str.equals("POWER_DOWN"))
  {
    return;
  }
  if (str.equals("PREPARE_MS"))
  {
    return;
  }
  if (str.equals("RESPONSE_MS"))
  {
    return;
  }
  if (str.equals("VCC_MV"))
  {
    return;
  }
  if (str.equals("ASSIM_ROLE"))
  {
    slave.role = (strcmp(packet, "SENSOR") == 0) ? SENSOR : ACTOR;
    return;
  }
  if (str.equals("CLOCK_STRETCH"))
  {
    slave.clock_stretch = atoi(packet);
    return;
  }
  // end of base properties
  char *idx_chars = strtok(packet, ":|");
  String idx_str = String(idx_chars);
  int idx = atoi(idx_chars);
  if (!address_json_obj[idx_str])// only creates first
  {
    address_json_obj.createNestedObject(idx_str);
  }
  JsonObject& idx_json_obj = address_json_obj[idx_str].as<JsonObject>();
  if (str.equals("VIZ_CARD_TYPE"))
  {
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["card_type"] = String(value);
    set_defaults_for_card_type(value, idx_json_obj);
    return;
  }
  if (str.equals("VIZ_ICONS"))
  {
    // break up idx, name, value
    //idx = atoi(strtok(packet, ":|"));
    name = strtok(NULL, ":|");
    value = strtok(NULL, ":|");
	// json
    if (!idx_json_obj["icons"])
    {
      idx_json_obj.createNestedArray("icons");
    }
	JsonObject& idx_icon_json_obj = idx_json_obj["icons"].as<JsonArray>().createNestedObject();
	idx_icon_json_obj["name"] = String(name);
	idx_icon_json_obj["value"] = String(value);
	return;
  }
  if (str.equals("VIZ_LABELS"))
  {
    // break up idx, name, value
    //idx = atoi(strtok(packet, ":|"));
    name = strtok(NULL, ":|");
    value = strtok(NULL, ":|");
    // json
    if (!idx_json_obj["labels"])
    {
      idx_json_obj.createNestedArray("labels");
    }
	JsonObject& idx_label_json_obj = idx_json_obj["labels"].as<JsonArray>().createNestedObject();
	idx_label_json_obj["name"] = String(name);
	idx_label_json_obj["value"] = String(value);
	return;
  }
  if (str.equals("VIZ_MIN"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["min"] = atoi(value);
    return;
  }
  if (str.equals("VIZ_MAX"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["max"] = atoi(value);
    return;
  }
  if (str.equals("VIZ_UNITS"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["units"] = String(value);
    return;
  }
  if (str.equals("VIZ_TOTAL"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["total"] = atoi(value);
    return;
  }
  if (str.equals("VIZ_VALUES"))
  {
    // break up idx, name, value
    //idx = atoi(strtok(packet, ":|"));
    name = strtok(NULL, ":|");
    value = strtok(NULL, ":|");
    // json
    if (!idx_json_obj["values"])
    {
      idx_json_obj.createNestedObject("values");
    }
	if (idx_json_obj["values"].as<JsonArray>().size() == 2) {// remove the defaults - ToDo: more robust strategy
		idx_json_obj["values"].as<JsonArray>().removeAt(0);
		idx_json_obj["values"].as<JsonArray>().removeAt(0);
	}
	JsonObject& idx_value_json_obj = idx_json_obj["values"].as<JsonArray>().createNestedObject();
	idx_value_json_obj["name"] = String(name);
	idx_value_json_obj["value"] = String(value);
	return;
  }
  if (str.equals("VIZ_IS_SERIES"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["is_series"] = strcmp(value, "true") == 0;
	set_defaults_for_card_type("chart-line", idx_json_obj);
	return;
  }
  if (str.equals("VIZ_M_SERIES"))// MULTI SERIES - MULTIPLE LINES ON LINE CHASRT
  {

    return;
  }
  if (str.equals("VIZ_HIGH"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["high"] = atoi(value);
    return;
  }
  if (str.equals("VIZ_LOW"))
  {
    // break up idx, value
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["low"] = atoi(value);
    return;
  }
  if (str.equals("VIZ_TOTL_UNIT"))
  {
    // break up idx, total, unit
    //idx = atoi(strtok(packet, ":|"));
    value = strtok(NULL, ":|");
    // json
    idx_json_obj["total"] = atoi(value);
    units = strtok(NULL, ":|");
    idx_json_obj["units"] = String(units);
    return;
  }
}

void AssimilateBus::get_properties(NameValueCallback nvCallback, SlavesProcessedCallback spCallback){
	_debug_assim.out_fla(F("[I2C REQUEST SENSOR VALUES]\tSLAVE_ADDR\tPROP_IDX\tSEGMENT_IDX"), true, 1);
	for (byte index = 0; index < _deviceCount; index++)
	{
		get_property(_assimSlaves[index], nvCallback);
	}// end for index	
	spCallback();
}

void AssimilateBus::get_property(Slave slave, NameValueCallback nvCallback) {
	char packet[16];
	char name[16];
	char value[16];
	byte prop_index = 0;
	while (true)
	{
		bool i2cNodeProcessed = false;
		for (byte segment = 0; segment < 3; segment++)
		{
			_debug_assim.out_fla(F("[I2C REQUEST SENSOR VALUES]\t\t"), false, 1);
			_debug_assim.out_str(String(slave.address), false, 1);
			_debug_assim.out_fla(F("\t\t"), false, 1);
			_debug_assim.out_str(String(prop_index), false, 1);
			_debug_assim.out_fla(F("\t\t"), false, 1);
			_debug_assim.out_str(String(segment), true, 1);
			Wire.setClockStretchLimit(slave.clock_stretch);
			Wire.requestFrom(slave.address, 16);
			byte writeIndex = 0;
			while (Wire.available()) { // slave may send less than requested
				const char c = Wire.read();
				int intOfChar = int(c);
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
				nvCallback(slave.address, prop_index, slave.role, name, value);
				prop_index++;
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
}

void AssimilateBus::print_metadata_details(){
	Serial.print(F("DEVICE COUNT: "));
	Serial.println(_deviceCount);
	for (byte index = 0; index < _deviceCount; index++)
	{
		Serial.print(_assimSlaves[index].address);
		Serial.print(F("\t"));
		Serial.print(_assimSlaves[index].name);
		Serial.print(F("\t"));
		Serial.print(_assimSlaves[index].role == ACTOR ? "ACTOR" : "SENSOR");
		Serial.print(F("\t"));
		Serial.println(_assimSlaves[index].clock_stretch);
	}	
}

void AssimilateBus::scan_bus_for_slave_addresses(){
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

void AssimilateBus::send_to_actor(byte address, byte code, const char *param){
	delay(1);
	Wire.beginTransmission(address);
	Wire.write(code);
	Wire.write(param);
	Wire.endTransmission();
	delay(100);
}

byte AssimilateBus::parse_prop_index(const char* value, int position){
	char buff[64];
	strcpy(buff, value);
	char *idx1_chars = strtok(buff, ":");
	if (idx1_chars == NULL)	{
		return -1;
	}
	if (position == 0)	{
		return atoi(idx1_chars);
	}
	char *idx2_chars = strtok(NULL, ":");
	if (idx2_chars == NULL) {
		return -1;
	}
	return atoi(idx2_chars);
}

void AssimilateBus::set_defaults_for_card_type(char *card_type, JsonObject &idx_json_obj) {
  if (strcmp("toggle", card_type) == 0)  {
	  return;
  }
  if (strcmp("input", card_type) == 0)  {
	  return;
  }
  if (strcmp("text", card_type) == 0)  {
    idx_json_obj["units"] = "STATUS";
    return;
  }
  if (strcmp("slider", card_type) == 0)  {
	  return;
  }
  if (strcmp("button", card_type) == 0)  {
	  return;
  }
  if (strcmp("chart-donut", card_type) == 0)  {
    idx_json_obj["total"] = 100;
    idx_json_obj["units"] = "%";
	if (!idx_json_obj["values"])	{
		idx_json_obj.createNestedArray("values");
	}
	JsonObject& value_json_obj1 = idx_json_obj["values"].as<JsonArray>().createNestedObject();
	value_json_obj1["name"] = "labels";
	value_json_obj1["value"] = "[]";
	JsonObject& value_json_obj2 = idx_json_obj["values"].as<JsonArray>().createNestedObject();
	value_json_obj2["name"] = "series";
	value_json_obj2["value"] = "0";
    return;
  }
  if (strcmp("chart-line", card_type) == 0)  {
    idx_json_obj["max"] = 12;
    idx_json_obj["low"] = 0;
    idx_json_obj["high"] = 100;
    return;
  }
}

bool AssimilateBus::get_user_meta_or_default(const JsonArray& json_array, char name[16], char packet[64]){
	// only interested in VIZ_ metadata!
	if (strncmp(name, "VIZ_", 4)) return false;
	// get index of slave metadata of interest
	const byte slave_prop_index1 = parse_prop_index(packet, 0);
	const byte slave_prop_index2 = parse_prop_index(packet, 1);
	for (byte i = 0; i < json_array.size(); i++) {
		bool do_copy = true;
		if (strcmp(json_array[i]["name"].as<char*>(), name) == 0) {
			//get index of user metadata of interest
			const char* json_value = json_array[i]["value"].as<char*>();
			const byte user_prop_index1 = parse_prop_index(json_value, 0);
			const byte user_prop_index2 = parse_prop_index(json_value, 1);
			if (slave_prop_index1 == user_prop_index1) {
				if (strcmp(name, "VIZ_ICONS") == 0){ // needs a match on both indexes
					if (slave_prop_index2 == user_prop_index2) {
						strcpy(packet, json_value);
						return true;
					} else {
						do_copy = false;
					}
				}
				if (do_copy){
					strcpy(packet, json_value);
					return true;
				}
			}
		}
	}
	return false;
}

AssimilateBus::Slave AssimilateBus::get_slave_by_address(byte address)
{
	for (byte i = 0; i < _deviceCount; i++)
	{
		if (_assimSlaves[i].address == address)
		{
			return _assimSlaves[i];
		}
	}
	return _defaultSlave; // should not hit here
}