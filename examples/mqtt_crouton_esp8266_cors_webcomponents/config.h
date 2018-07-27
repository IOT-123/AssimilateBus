/*
config.h - Library for SPIFFS and config file. 
Created by Nic Roche, May 23, 2018. 
Released into the public domain.
*/

#ifndef _CONFIG_h
#define _CONFIG_h


#include "types.h"
#include <arduino.h>
#include <ArduinoJson.h>



class Config
{
public:
	const char* FILE_CONFIG_DEVICE = "/config/device.json";
	const char* FILE_CONFIG_USER_METAS = "/config/user_metas_%i.json";
	const char* FILE_CONFIG_USER_PROPS = "/config/user_props.json";
	const char* FILE_CONFIG_USER_CARD = "/config/user_card_%s.json";
	const char* DIR_CONFIG_SLAVE_METAS = "/config/slave_metas_%i.json";
	bool get_device_data(SetupDeviceData& setup_data, RuntimeDeviceData& runtime_data);
	bool write(char buffer[], const char* path);
	JsonObject& get_slave_meta_json_object(byte slave_address);
	bool set_slave_meta_json(byte slave_address, JsonObject& json_obj);
	void clear_metadata_dir();
	void update_prop_dtos(PropertyDto dto_props[], byte sensor_count);
	String read(const char* path);
	String get_user_card(const char* file_id);
private:
};


#endif

