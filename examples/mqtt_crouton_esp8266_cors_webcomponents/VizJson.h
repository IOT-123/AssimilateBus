/*
viz_json.h - Library for building Crouton data structs, but can be modified for raw data easily.
Created by Nic Roche, May 29, 2018.
Released into the public domain.
*/

#ifndef _VIZ_JSON_h
#define _VIZ_JSON_h

#include <ArduinoJson.h>
#include "assimilate_bus.h"
#include "config.h"

class VizJson
{
public:
	VizJson();
	String build_device_info(PropertyDto *dto_props, int sensor_count, char *mqtt_device_name, char *mqtt_device_description, char *viz_color);
	String str_replace(String name, String search, String relacement);
	String format_update_json(char *card_type, char *label, char *value);
	String format_endpoint_name(char name[16]);
	const String POSTFIX_SERIES = "_series";
	const String PREFIX_CUSTOM_CARD = "CC_";
private:
//	bool addCustomEndpoint(JsonObject& deviceInfo_endPoints, String endpoint_name, const char *file_id, char *mqtt_device_name);
	String read_custom_endpoint(const char *file_id, char *mqtt_device_name);
	char* get_viz_card_type(char *str);
	const char* bool_str_or_default(const char str[16]);
	bool is_numeric(char *str);
	const char* get_json_key(JsonObject& json_obj, byte index);
	String get_json_value(JsonObject& json_obj, byte index);
	JsonObject& get_json_value_object(JsonObject& json_obj, byte index);
	Config _config;
	byte get_card_count(PropertyDto *dto_props, int sensor_count);
};

#endif
