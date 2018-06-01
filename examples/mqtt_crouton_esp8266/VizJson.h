/*
viz_json.h - Library for building Crouton data structs, but can be modified for raw data easily.
Created by Nic Roche, May 29, 2018.
Released into the public domain.
*/

#ifndef _VIZ_JSON_h
#define _VIZ_JSON_h

#include <AssimilateBus.h>
#include <ArduinoJson.h>

class VizJson
{
public:
	VizJson();
	String build_device_info(NameValuePropViz *_sensor_details, int sensor_count, char *mqtt_device_name, char *mqtt_device_description, char *viz_color);
	String str_replace(String name, String search, String relacement);
  String format_update_json(char *card_type, char *label, char *value);
  const String POSTFIX_SERIES = "_series";
private:
	char* get_viz_card_type(char *str);
	char* bool_str_or_default(char str[16]);
  bool is_numeric(char *str);
};

#endif
