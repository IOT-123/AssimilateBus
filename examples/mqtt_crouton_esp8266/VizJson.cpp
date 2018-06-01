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
		bool has_labels, has_icons, has_values, is_toggle, is_donut, has_total, has_units;
		if (strcmp("toggle", _sensor_details[i].prop_vizs.card_type) == 0)
		{
			has_labels = true;
			has_icons = true;
			has_values = true;
			is_toggle = true;
      is_donut = false;
      has_total = false;
      has_units = false;
		}else if (strcmp("chart-donut", _sensor_details[i].prop_vizs.card_type) == 0)
    {
      has_labels = false;
      has_icons = false;
      is_toggle = false;
      has_values = true;
      is_donut = true;
      has_total = true;
      has_units = true;
    }
		String json_name = format_endpoint_name(_sensor_details[i].name);
    char* card_type = get_viz_card_type(_sensor_details[i].prop_vizs.card_type);






    
    if (strcmp(card_type, "unsupported") != 0){
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
  Serial.println("has_values");
        JsonObject& endpoint_values = endpoint.createNestedObject("values");
        for (int j = 0; j < 3; j++)
        {
          if (strcmp(_sensor_details[i].prop_vizs.values[j].name, "") != 0) {
            if (is_toggle){
              endpoint_values[_sensor_details[i].prop_vizs.values[j].name] = *_sensor_details[i].prop_vizs.values[j].value == '0';
            }else if (is_donut){
              if (strcmp("[]", _sensor_details[i].prop_vizs.values[j].value)==0){
                endpoint_values.createNestedArray(_sensor_details[i].prop_vizs.values[j].name);
              }else{
                endpoint_values[_sensor_details[i].prop_vizs.values[j].name] = String(_sensor_details[i].prop_vizs.values[j].value).toInt();
              }
            }
          }
        }
      }
		  if (has_total){
        endpoint["total"] = String(_sensor_details[i].prop_vizs.total).toInt();
		  }
     if (has_units){
      endpoint["units"] = _sensor_details[i].prop_vizs.units;
     }
     if (is_donut){
      endpoint["centerSum"] = true;
     }
		}
    if (_sensor_details[i].prop_vizs.is_series){
/*      
"temperature": {
    "values": {
        "labels": [1],  [required]
        "series": [[60]],  [required]
        "update": null  [required]  
    },
    "max": 11, [required]
    "low": 58,  [required]
    "high": 73,  [required]
    "card-type": "crouton-chart-line",  [required]
    "title": "Temperature (F)" [optional]
}      
 */
 //char *concat;
 //sprintf(concat, "%s [series]", _sensor_details[i].name);
      JsonObject& series_endpoint = deviceInfo_endPoints.createNestedObject(json_name + POSTFIX_SERIES); // THIS NEEDS TO BE STRIPPED
      series_endpoint["title"] = _sensor_details[i].name;
      series_endpoint["card-type"] = "crouton-chart-line";
      
      series_endpoint["max"] = _sensor_details[i].prop_vizs.max;
      series_endpoint["low"] = _sensor_details[i].prop_vizs.low;
      series_endpoint["high"] = _sensor_details[i].prop_vizs.high;

      
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

