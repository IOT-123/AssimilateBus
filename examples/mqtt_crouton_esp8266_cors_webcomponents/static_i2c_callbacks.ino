void on_bus_received(byte slave_address, byte prop_index, Role role, char name[16], char value[16]){
	_debug.out_fla(F("on_bus_received"), true, 2);
	if (!_sent_device_info)
	{
		// ToDo: this gets multiple calls to same file every slave - maybe a lite cache? could be global memory hit!
		// get the dtos until send of deviceInfo
		if (!get_json_card_type(slave_address, prop_index, _dto_props[_dto_props_index].card_type)){
			// not a property confugured in metadata for Crouton
			return;
		}
		_dto_props[_dto_props_index].is_series = get_json_is_series(slave_address, prop_index);
		_dto_props[_dto_props_index].slave_address = slave_address;
		_dto_props[_dto_props_index].prop_index = prop_index;
		strcpy(_dto_props[_dto_props_index].name, name);
		strcpy(_dto_props[_dto_props_index].value, value);
		_dto_props[_dto_props_index].role = role;
		_dto_props_index++;
		return;
	}
	// values not to publish
	if (strcmp(value, "HEATING")==0){
		_debug.out_fla(F("NOT PUBLISHING HEATING"), true, 2);
		return;
	}
	if (strcmp(value, "CALIBRATING")==0){
		_debug.out_fla(F("NOT PUBLISHING CALIBRATING"), true, 2);  
		return;
	}
	char card_type[16]; // this is the only metadata value used, others come from property definitions
	if (!get_struct_card_type(slave_address, prop_index, card_type)){
		// properties are defined in slave but visualization metadata does not exist
		// the value can still be pumped to MQTT, but you will need to supply a topic
		return; 
	}
	String formatted_json = _viz_json.format_update_json(card_type, "", value);
	String endpoint_name = _viz_json.format_endpoint_name(name);
	mqtt_publish(_mqtt_pub_topic, _runtime_device_data.mqtt_device_name, const_cast<char*>(endpoint_name.c_str()), formatted_json.c_str());
	if (get_json_is_series(slave_address, prop_index)){// as chart-line
		formatted_json = _viz_json.format_update_json("chart-line", time_stamp_get(), value);
		String series_endpoint = endpoint_name + _viz_json.POSTFIX_SERIES;
		mqtt_publish(_mqtt_pub_topic, _runtime_device_data.mqtt_device_name, const_cast<char*>(series_endpoint.c_str()), formatted_json.c_str());
	}
}

void on_bus_complete(){
	_debug.out_fla(F("on_bus_complete"), true, 2);
	if (_sent_device_info)	{
		_debug.out_fla(F("_sent_device_info return"), true, 2);
		return;
	}
	_debug.out_fla(F("onBusComplete !_sent_device_info"), true, 2);
	// update names from user_props.json
	_config_data.update_prop_dtos(_dto_props, _dto_props_index);
	String payload = _viz_json.build_device_info(_dto_props, _dto_props_index, _runtime_device_data.mqtt_device_name, _runtime_device_data.mqtt_device_description, _runtime_device_data.viz_color);
	//Serial.println(payload);
	mqtt_publish(_mqtt_pub_topic, _runtime_device_data.mqtt_device_name, "deviceInfo", payload.c_str());
	// after sending the deviceInfo request the sensors again - happens first time only
	_sent_device_info = true;
	_assimilate_bus.get_properties(on_bus_received, on_bus_complete);
	mqtt_create_subscriptions(); //ToDo: use endpoint_alias from _dto_props for extra subscriptions
}
