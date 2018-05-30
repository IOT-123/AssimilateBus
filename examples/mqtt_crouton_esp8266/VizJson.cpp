/*
viz_json.h - Library for building Crouton data structs, but can be modified for raw data easily.
Created by Nic Roche, May 29, 2018.
Released into the public domain.
*/

#include "VizJson.h"

VizJson::VizJson(){}

//called with each reboot
String VizJson::build_device_info(NameValuePropViz *_sensor_details, int sensor_count, char *mqtt_device_name, char *mqtt_device_description, char *viz_color) {
	Serial.println("build_device_info START");
	int bufferSize = 250 + (sensor_count * 350);
	DynamicJsonBuffer jsonBuffer(bufferSize);
	JsonObject& root = jsonBuffer.createObject();
	JsonObject& deviceInfo = root.createNestedObject("deviceInfo");
	JsonObject& deviceInfo_endPoints = deviceInfo.createNestedObject("endPoints");
	for (int i = 0; i < sensor_count; i++)
	{
		bool has_labels, has_icons, has_values, is_toggle;
		if (strcmp("toggle", _sensor_details[i].prop_vizs.card_type) == 0)
		{
			has_labels = true;
			has_icons = true;
			has_values = true;
			is_toggle = true;
		}
		String json_name = str_replace(String(_sensor_details[i].name), " ", "_");
		json_name = str_replace(json_name, "%", "PC");
		json_name = str_replace(json_name, "(", "");
		json_name = str_replace(json_name, ")", "");
		json_name = str_replace(json_name, "/", "_");
		json_name = str_replace(json_name, ",", "_");
		json_name.toLowerCase();
		JsonObject& endpoint = deviceInfo_endPoints.createNestedObject(json_name); // THIS NEEDS TO BE STRIPPED
		endpoint["title"] = _sensor_details[i].name;
		endpoint["card-type"] = get_viz_card_type(_sensor_details[i].prop_vizs.card_type);
		if (has_labels)
		{
			JsonObject& endpoint_labels = endpoint.createNestedObject("labels");
			char *label1 = bool_str_or_default(_sensor_details[i].prop_vizs.labels[0].name);
			endpoint_labels[label1] = _sensor_details[i].prop_vizs.labels[0].value;
			char *label2 = bool_str_or_default(_sensor_details[i].prop_vizs.labels[1].name);
			endpoint_labels[label2] = _sensor_details[i].prop_vizs.labels[1].value;
		}
		if (has_icons)
		{
			JsonObject& endpoint_icons = endpoint.createNestedObject("icons");
			char *icon1 = bool_str_or_default(_sensor_details[i].prop_vizs.icons[0].name);
			endpoint_icons[icon1] = _sensor_details[i].prop_vizs.icons[0].value;
			char *icon2 = bool_str_or_default(_sensor_details[i].prop_vizs.icons[1].name);
			endpoint_icons[icon2] = _sensor_details[i].prop_vizs.icons[1].value;
		}
		if (has_values)
		{
			JsonObject& endpoint_values = endpoint.createNestedObject("values");
			for (int i = 0; i < 3; i++)
			{
				if (strcmp(_sensor_details[i].prop_vizs.values[i].name, "") != 0) {
					if (is_toggle)
					{
						endpoint_values[_sensor_details[i].prop_vizs.values[i].name] = *_sensor_details[i].prop_vizs.values[i].value == '0';
					}
				}
			}
		}
	}
	deviceInfo["status"] = "good";
	deviceInfo["name"] = mqtt_device_name;
	deviceInfo["description"] = mqtt_device_description;
	deviceInfo["color"] = viz_color;
	String output;
	root.printTo(Serial);
	root.printTo(output);
	Serial.println("build_device_info END");
	return output;
}

//not for the purist :)
String VizJson::str_replace(String name, String search, String relacement)
{
	String str = name;
	str.replace(search, relacement);
	return str;
}

// progmem/i2c on slaves did not allow long fields
char* VizJson::get_viz_card_type(char *str)
{
	if (strcmp("toggle", str) == 0)
	{
		return "crouton-simple-toggle";
	}
	if (strcmp("input", str) == 0)
	{
		return "crouton-simple-input";
	}
	if (strcmp("text", str) == 0)
	{
		return "crouton-simple-text";
	}
	if (strcmp("slider", str) == 0)
	{
		return "crouton-simple-slider";
	}
	if (strcmp("button", str) == 0)
	{
		return "crouton-simple-button";
	}
	if (strcmp("chart-donut", str) == 0)
	{
		return "crouton-chart-donut";
	}
	if (strcmp("chart-line", str) == 0)
	{
		return "crouton-chart-line";
	}
	return "unsupported";
}

// Crouton is partial to bools
char* VizJson::bool_str_or_default(char str[16])
{
	if (strcmp("0", str) == 0)
	{
		return "false";
	}
	if (strcmp("1", str) == 0)
	{
		return "true";
	}
	return str;
}