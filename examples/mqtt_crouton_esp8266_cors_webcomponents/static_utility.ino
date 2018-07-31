
String spiffs_file_list_build(String path){
  Dir dir = SPIFFS.openDir(path);
  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  return output;
}

void report_deserialize_error(){
  _debug.out_fla(F("ERROR SPIFFS DESERIALIZE /config/device.json"), true, 0);
  _fatal_error = true;
}

void report_spiffs_error(){
  _debug.out_fla(F("ERROR SPIFFS BEGIN"), true, 0);
  _fatal_error = true;
}

bool check_fatal_error(){
  if (_fatal_error){
    _debug.out_fla(F("ERROR: Fatal - no functions (maybe do a restart here)"), true, 0);
    delay(10000);
    return false;
  }
  return true;
}

bool get_json_card_type(byte slave_address, byte prop_index, char *card_type){
  JsonObject& slave_json_obj = _config_data.get_slave_meta_json_object(slave_address);
  if (!slave_json_obj[String(prop_index)]){
    return false;
  }
  strcpy(card_type, slave_json_obj[String(prop_index)]["card_type"]);
  return true;
}

bool get_struct_card_type(byte slave_address, byte prop_index, char *card_type){
 for (byte i = 0; i < sizeof(_dto_props)/sizeof(PropertyDto); i++){
    if (_dto_props[i].slave_address == slave_address && _dto_props[i].prop_index == prop_index){
      strcpy(card_type, _dto_props[i].card_type);  
      return true;    
    }
  }
  return false;
}

byte get_prop_dto_idx(byte slave_address, byte prop_index)
{
	for (byte i = 0; i < sizeof(_dto_props) / sizeof(PropertyDto); i++) {
		if (_dto_props[i].slave_address == slave_address && _dto_props[i].prop_index == prop_index) {
			return i;
		}
	}
	return -1;
}

bool get_json_is_series(byte slave_address, byte prop_index){
  JsonObject& slave_json_obj = _config_data.get_slave_meta_json_object(slave_address);
  if (!slave_json_obj[String(prop_index)]){
    return false;
  }
  return slave_json_obj[String(prop_index)]["is_series"];
}

void str_replace(char *src, const char *oldchars, char *newchars) { // utility string function
	char *p = strstr(src, oldchars);
	char buf[16];
	do {
		if (p) {
			memset(buf, '\0', strlen(buf));
			if (src == p) {
				strcpy(buf, newchars);
				strcat(buf, p + strlen(oldchars));
			}
			else {
				strncpy(buf, src, strlen(src) - strlen(p));
				strcat(buf, newchars);
				strcat(buf, p + strlen(oldchars));
			}
			memset(src, '\0', strlen(src));
			strcpy(src, buf);
		}
	} while (p && (p = strstr(src, oldchars)));
}
