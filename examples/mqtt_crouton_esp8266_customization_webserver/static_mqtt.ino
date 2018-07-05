long _last_reconnect_attempt = 0;

void mqtt_loop(){
  if (!_client.connected()) {
	  long now = millis();
	  if (now - _last_reconnect_attempt > 5000) {
		  _last_reconnect_attempt = now;
		  // Attempt to reconnect
		  if (mqtt_ensure_connect()) {
			  _last_reconnect_attempt = 0;
		  }
	  }
  }
  _client.loop();
} // mqtt_loop())

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  _debug.out_fla(F("mqtt_callback"), true, 2);
  char payload_chars[50];
  String slave_payload;
  _debug.out_char(topic, true, 2);
  for (int i = 0; i < length; i++) {
    payload_chars[i] = (char)payload[i];
  }
  _debug.out_char(payload_chars, true, 2);
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload_chars);
  if (!root.success()) {
    _debug.out_fla(F("mqtt_callback parseObject() failed"), true, 0);
  }else
  {
#if(DBG_OUTPUT_FLAG==2)
    root.printTo(Serial);
    Serial.println();
#endif    
  }
  // break up idx, value
  char *root_topic = strtok(topic, "/");
  char *device = strtok(NULL, "/");
  char *property = strtok(NULL, "/");
  byte address = -1;
  char card_type[16];
  for (byte i = 0; i < sizeof(_dto_props) / sizeof(PropertyDto); i++) {
    String formatted_property = _viz_json.format_endpoint_name(_dto_props[i].name);
    if (strcmp(property, formatted_property.c_str()) == 0){
      address = _dto_props[i].slave_address;
      strcpy(card_type, _dto_props[i].card_type);
      break;
    }
  }
  if (strcmp("toggle", card_type) == 0) {
    int8_t topic_index = mqtt_get_topic_index(property);
    if (topic_index == -1){
        slave_payload = root["value"] ? "1" : "0";
    } else { // indexed properties like 2CH RELAY
        slave_payload = root["value"] ? "1" : "0";      
        slave_payload = String(topic_index) + ":" + slave_payload;
    }
    _assimilate_bus.send_to_actor(address, 2, slave_payload.c_str());          
    return;
  }
  if (strcmp("input", card_type) == 0){
    _assimilate_bus.send_to_actor(address, 2, root["value"]);// limit to 16?
    return;
  }
  //if (strcmp("text", card_type) == 0)
  //{
  //  //na
  //}
  if (strcmp("slider", card_type) == 0) {
    _assimilate_bus.send_to_actor(address, 2, root["value"]);// limit to 16?
    return;
  }
  if (strcmp("button", card_type) == 0) {
    if (root["value"])
    {
      _assimilate_bus.send_to_actor(address, 2, "1");
    } else {
      _assimilate_bus.send_to_actor(address, 2, "0");
    }
    return;
  }
} // mqtt_callback()

int8_t mqtt_get_topic_index(char* topic){
    char *property = strtok(topic, "[]");
    char *index = strtok(NULL, "[]");
    if (index == NULL){
      return -1;
    }
    return atoi(index);
} // mqtt_get_topic_index()

void mqtt_subscribe(char *root_topic, char *deviceName, char *endpoint){
  char topic[127];
  sprintf(topic, "/%s/%s/%s", root_topic, deviceName, endpoint);
  _client.subscribe(topic);
} // mqtt_subscribe()

//_runtime_device_data.mqtt_device_name
void mqtt_create_subscriptions(){
  _debug.out_fla(F("mqtt_create_subscriptions"), true, 2);
  if (_dto_props_index == 0) // the sensors/actors have not been logged
  {
    return;
  }
  for (int i = 0; i < _dto_props_index; i++)
  {
    if (strcmp("", _dto_props[i].name) != 0){ // if assigned
      if (_dto_props[i].role == ACTOR){ // only actors
        String endpoint_name = _viz_json.format_endpoint_name(_dto_props[i].name);
        mqtt_subscribe(_mqtt_sub_topic, _runtime_device_data.mqtt_device_name, const_cast<char*>(endpoint_name.c_str()));
      }
    }
  }
} // mqtt_create_subscriptions()

//_runtime_device_data.mqtt_device_name
bool mqtt_ensure_connect(){
  char lwt_topic[127];
  sprintf(lwt_topic, "/%s/%s/%s", _mqtt_pub_topic, _runtime_device_data.mqtt_device_name, "lwt");
  while (!_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    bool connect_success;
	uint32_t chip_id = ESP.getChipId();
	char client_id[25];
	snprintf(client_id, 25, "ESP8266-%08X", chip_id);
    if ((strcmp(_runtime_device_data.mqtt_username, "") == 0))// if not credentials
    {
      connect_success = _client.connect(client_id, lwt_topic, 0, false, "anything for last will and testament");
    }else // if credentials
    {
      connect_success = _client.connect(client_id, _runtime_device_data.mqtt_username, _runtime_device_data.mqtt_password, lwt_topic, 0, false, "anything for last will and testament");
    }
    if (connect_success) {
      Serial.println("connected");
      mqtt_create_subscriptions();
	  return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(_client.state());
      Serial.println(" will try again in 5 seconds"); // non-blocking delay in mqtt_loop()
	  return false;
    }
  }
} // mqtt_ensure_connect()

void mqtt_init(const char* wifi_ssid, const char* wifi_password, const char* mqtt_broker, int mqtt_port){
  WiFi.mode(WIFI_STA);
  delay(10);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print(F("[WIFI CONNECTION] "));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  _client.setServer(mqtt_broker, mqtt_port);
  _client.setCallback(mqtt_callback);
  Serial.println();
} // mqtt_init()

void mqtt_publish(char *root_topic, char *deviceName, char *endpoint, const char *payload){
  Serial.print(F("[MQTT SEND] "));
  Serial.println(payload);
  if (strcmp(payload, "")==0){
    return;
  }
  if (strcmp(endpoint, "")==0){
    return;
  }
  char topic[127];
  sprintf(topic, "/%s/%s/%s", root_topic, deviceName, endpoint);
  _client.publish(topic, payload);
} // mqtt_publish()
