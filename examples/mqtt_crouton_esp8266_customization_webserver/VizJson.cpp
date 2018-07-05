/*
viz_json.h - Library for building Crouton data structs, but can be modified for raw data easily.
Created by Nic Roche, May 29, 2018.
Released into the public domain.
*/

#include "VizJson.h"


VizJson::VizJson(){}

String VizJson::format_endpoint_name(char name[16]){
    String endpoint_name = str_replace(String(name), " ", "_");
    endpoint_name = str_replace(endpoint_name, "%", "PC");
    endpoint_name = str_replace(endpoint_name, "(", "");
    endpoint_name = str_replace(endpoint_name, ")", "");
    endpoint_name = str_replace(endpoint_name, "/", "_");
    endpoint_name = str_replace(endpoint_name, ",", "_");
    endpoint_name = str_replace(endpoint_name, ".", "");
    endpoint_name.toLowerCase();
    return endpoint_name;
}

byte VizJson::get_card_count(PropertyDto *dto_props, int sensor_count) {
	byte counter = 0;
	for (int i = 0; i < sensor_count; i++)	{
		counter++;
		if (dto_props[i].is_series) {
			counter++;
		}
	}
	return counter;
}

//called with each reboot
String VizJson::build_device_info(PropertyDto *dto_props, int sensor_count, char *mqtt_device_name, char *mqtt_device_description, char *viz_color) {
Serial.println(F("[DEVICEINFO BUILD] LOAD USER CUSTOMIZATIONS"));
	byte card_count = get_card_count(dto_props, sensor_count);
	int bufferSize = 250 + (card_count * 350);
	DynamicJsonBuffer json_create_buffer_buffer(bufferSize);
	JsonObject& root = json_create_buffer_buffer.createObject();
	JsonObject& deviceInfo = root.createNestedObject("deviceInfo");
	JsonObject& deviceInfo_endPoints = deviceInfo.createNestedObject("endPoints");
  for (int i = 0; i < sensor_count; i++)
  {
    bool has_labels, has_icons, has_values, is_toggle, is_donut, has_total, has_units;
    String slave_address = String(dto_props[i].slave_address);
    String prop_index = String(dto_props[i].prop_index);
    JsonObject& slave_json_obj = _config.get_slave_meta_json_object(dto_props[i].slave_address);
    JsonObject& prop_json_obj = slave_json_obj[prop_index];
    String card_type_short = prop_json_obj["card_type"];
	if (card_type_short == "toggle"){
      has_labels = true;
      has_icons = true;
      has_values = true;
      is_toggle = true;
      is_donut = false;
      has_total = false;
      has_units = false;
    }else if (card_type_short == "chart-donut"){ 
      has_labels = false;
      has_icons = false;
      is_toggle = false;
      has_values = true;
      is_donut = true;
      has_total = true;
      has_units = true;
    }else if (card_type_short == "text"){   
      has_labels = false;
      has_icons = false;
      is_toggle = false;
      is_donut = false;
      has_values = true;
      has_total = false;
      has_units = true;
    }
	String json_name = format_endpoint_name(dto_props[i].name);
	char *card_type = get_viz_card_type(const_cast<char*>(card_type_short.c_str()));
	if (strcmp(card_type, "unsupported") != 0){
		JsonObject& endpoint = deviceInfo_endPoints.createNestedObject(json_name);
		endpoint["title"] = String(dto_props[i].user_prop_name);
      endpoint["card-type"] = card_type;
	  if (has_labels) {
		  JsonObject& endpoint_labels = endpoint.createNestedObject("labels");
		  for (byte i = 0; i < prop_json_obj["labels"].size(); i++) {
			  const char *label1 = bool_str_or_default(prop_json_obj["labels"][i]["name"]);
			  endpoint_labels[label1] = prop_json_obj["labels"][i]["value"].as<String>();
		  }
	  }
      if (has_icons){
		JsonObject& endpoint_labels = endpoint.createNestedObject("icons");
		for (byte i = 0; i < prop_json_obj["icons"].size(); i++) {
			const char *label1 = bool_str_or_default(prop_json_obj["icons"][i]["name"]);
			endpoint_labels[label1] = prop_json_obj["icons"][i]["value"].as<String>();
		}
      }
      if (has_values)
      {   
		JsonObject& endpoint_values = endpoint.createNestedObject("values");
        if (is_toggle){ // button also
          endpoint_values["value"] = prop_json_obj["values"]["value"].as<String>() == "0"; 
        } else if (is_donut){
          endpoint_values.createNestedArray("labels");
          endpoint_values["series"] = prop_json_obj["values"]["series"].as<int>();
        } else if (prop_json_obj["values"]["value"].as<int>()){ // slider
          endpoint_values["value"] = prop_json_obj["values"]["value"].as<int>();
        } else {// text, input
          endpoint_values["value"] = prop_json_obj["values"]["value"].as<String>();
        }
      }
      if (has_total){
		  endpoint["total"] = prop_json_obj["total"].as<int>();
      }
     if (has_units){
		 endpoint["units"] = prop_json_obj["units"].as<String>();
     }
     if (is_donut){
		 endpoint["centerSum"] = true;
     }
    }
    bool is_series = prop_json_obj["is_series"];
    if (is_series){
		JsonObject& series_endpoint = deviceInfo_endPoints.createNestedObject(json_name + POSTFIX_SERIES); // THIS NEEDS TO BE STRIPPED
      series_endpoint["title"] = dto_props[i].user_prop_name;
      series_endpoint["card-type"] = "crouton-chart-line";
      series_endpoint["max"] =  prop_json_obj["max"].as<int>();
      series_endpoint["low"] =  prop_json_obj["low"].as<int>();
      series_endpoint["high"] =  prop_json_obj["high"].as<int>(); 
      JsonObject& series_endpoint_values = series_endpoint.createNestedObject("values");
      JsonArray& series_endpoint_values_labels = series_endpoint_values.createNestedArray("labels");
      series_endpoint_values_labels.add(1);
      JsonArray& series_endpoint_values_series = series_endpoint_values.createNestedArray("series");
      JsonArray& series_endpoint_values_series_0 = series_endpoint_values_series.createNestedArray();
      series_endpoint_values_series_0.add(60);
      series_endpoint_values["update"] = "";      
	}
  }
  deviceInfo["status"] = "good";
  deviceInfo["name"] = mqtt_device_name;
  deviceInfo["description"] = mqtt_device_description;
  deviceInfo["color"] = viz_color;
  String output;
//root.printTo(Serial);
//  Serial.println();
  root.printTo(output);
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
const char* VizJson::bool_str_or_default(const char str[16])
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

// the json that is published periodically to update the visualizations
String VizJson::format_update_json(char *card_type, char *label, char *value){

  if (strcmp("toggle", card_type) == 0) // {"value": true}
  {
    return "{\"value\": " + String(bool_str_or_default(value)) + "}";
  }
  if (strcmp("input", card_type) == 0)// {"value": "YO"}
  {
    return "{\"value\": \"" + String(value) + "\"}";
  }
  if (strcmp("text", card_type) == 0)// {"value":919}
  {
    if (is_numeric(value)){
      return "{\"value\": " + String(value) + "}";     
    }else{
      return "{\"value\": \"" + String(value) + "\"}";
    }
  }
  if (strcmp("slider", card_type) == 0)// {"value": 60}
  {
    if (is_numeric(value)){
      return "{\"value\": " + String(value) + "}";     
    }else{
      return "{\"value\": 0}";
    }  
  }
  if (strcmp("button", card_type) == 0)// {"value": false} Disabler
  {
    return "{\"value\": " + String(bool_str_or_default(value)) + "}";
  }
  if (strcmp("chart-donut", card_type) == 0)// {"series":[20]}
  {
    if (is_numeric(value)){
      return "{\"series\": [" + String(value) + "]}";     
    }else{
      return "{\"series\": [0]}";
    }  
  }
  if (strcmp("chart-line", card_type) == 0)// {"update": {"labels":[4031],"series":[[68]]}}
  {
    return "{\"update\": {\"labels\":[\"" + String(label) + "\"],\"series\":[[" + String(value) + "]]}}";
  }
  return "";
}

bool VizJson::is_numeric(char *str) {
  for (byte i = 0; str[i]; i++)
  {
    if (!isDigit(str[i]) && str[i] != '.') return false;
  }
  return true;
}

const char* VizJson::get_json_key(JsonObject& json_obj, byte index) {
  byte iterator_index = 0;
  for (auto kv : json_obj) {
    if (index == iterator_index){
      return kv.key;
    }
    iterator_index++;
  }
  return "";
}

String VizJson::get_json_value(JsonObject& json_obj, byte index) {
  byte iterator_index = 0;
  for (auto kv : json_obj) {
    if (index == iterator_index){
      return kv.value.as<String>();
    }
    iterator_index++;
  }
  return "";
}
