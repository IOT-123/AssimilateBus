/*
config.cpp - Library for SPIFFS and config file. 
Created by Nic Roche, May 23, 2018. 
Released into the public domain.
*/

#include "config.h"
#include <FS.h>
#include <ArduinoJson.h>

String Config::read(const char* path) {
	bool exists = SPIFFS.exists(path);
	if (!exists) {
    Serial.print(path);
		Serial.println(F(" - DOES NOT EXISTS"));
		return "";
	}
	File f = SPIFFS.open(path, "r");
	if (!f) {
    Serial.print(path);
    Serial.println(F(" - DOES NOT OPEN"));
		return "";
	}
	String json = f.readString();
	f.close();
	return json;
}

String Config::get_user_card(char * file_id)
{
	char path[31];
	sprintf(path, FILE_CONFIG_USER_CARD, file_id);
	return read(path);
}

bool Config::write(char buffer[], const char* path) {
	File f = SPIFFS.open(path, "w");
	if (!f) {
		return false;
	}
	f.print(buffer);
	f.close();
	return true;
}

bool Config::get_device_data(SetupDeviceData& setup_data, RuntimeDeviceData& runtime_data){
	String json = read(FILE_CONFIG_DEVICE);
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(json);
	if (root == JsonObject::invalid())
	{
		return false;
	}
	setup_data.sensor_interval = root["sensor_interval"];
	strcpy(setup_data.ntp_server_name, root["ntp_server_name"]);
	setup_data.time_zone = root["time_zone"];
	strcpy(setup_data.wifi_ssid, root["wifi_ssid"]);
	strcpy(setup_data.wifi_key, root["wifi_key"]);
	strcpy(setup_data.mqtt_broker, root["mqtt_broker"]);
	strcpy(runtime_data.www_auth_username, root["www_auth_username"]);
	strcpy(runtime_data.www_auth_password, root["www_auth_password"]);
	strcpy(runtime_data.www_auth_exclude_files, root["www_auth_exclude_files"]);  
	strcpy(runtime_data.mqtt_username, root["mqtt_username"]);
	strcpy(runtime_data.mqtt_password, root["mqtt_password"]);
	setup_data.mqtt_port = root["mqtt_port"];
	strcpy(runtime_data.mqtt_device_name, root["mqtt_device_name"]);
	strcpy(runtime_data.mqtt_device_description, root["mqtt_device_description"]);
	strcpy(runtime_data.viz_color, root["viz_color"]);
	return true;
}


JsonObject& Config::get_slave_meta_json_object(byte slave_address){
  char path[31];
  sprintf(path, DIR_CONFIG_SLAVE_METAS, slave_address);
  String json = read(path);
  DynamicJsonBuffer jsonBuffer(360);
  JsonObject& root = jsonBuffer.parseObject(json);
  return root;
}

void Config::clear_metadata_dir(){
  for (byte i = 8; i < 128; i++){// range of i2c addresses
    char path[31];
    sprintf(path, DIR_CONFIG_SLAVE_METAS, i); 
    if (SPIFFS.exists(path)){    
      SPIFFS.remove(path);
    }
  }
}

bool Config::set_slave_meta_json(byte slave_address, JsonObject& json_obj){
  Serial.print(F("[METADATA SAVED] "));
   char buffer[512];
//json_obj.printTo(Serial);
  json_obj.printTo(buffer, sizeof(buffer));
  char path[31];
  sprintf(path, DIR_CONFIG_SLAVE_METAS, slave_address);
  Serial.println(path);
  return write(buffer, path);
}

void Config::update_prop_dtos(PropertyDto dto_props[], byte sensor_count) {
	Serial.println(F("[DEVICEINFO BUILD] UPDATE CROUTON CARD TITLES"));
	String json = read(FILE_CONFIG_USER_PROPS);
	DynamicJsonBuffer jsonBuffer((json.length() * 2) + 8);
	JsonObject& user_props_json_obj = jsonBuffer.parseObject(json);
	for (byte i = 0; i < sensor_count; i++) {
		bool user_value = false;
		String slave_address = String(dto_props[i].slave_address);
		String prop_index = String(dto_props[i].prop_index);
		if (user_props_json_obj.containsKey(slave_address) ){
			if (user_props_json_obj[slave_address].as<JsonObject>().containsKey(prop_index)) {
				user_value = true;
			}
		}
		if (user_value) {
			strcpy(dto_props[i].user_prop_name, user_props_json_obj[slave_address][prop_index].as<char*>());          
		}
		else {
			strcpy(dto_props[i].user_prop_name, dto_props[i].name);
		}
	}
}
