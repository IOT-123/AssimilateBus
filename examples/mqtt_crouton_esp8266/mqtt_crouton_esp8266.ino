#include "VizJson.h"
#include <AssimilateBus.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h> // set MQTT_MAX_PACKET_SIZE to ~400 (or your needs for deviceInfo json)

AssimilateBus _assimilate_bus;
VizJson _viz_json;
WiFiClient _esp_client;
PubSubClient _client(_esp_client);

/*
* VALUES TO UPDATE ON SPIFFS VIA SERIAL INPUT:
*	INDIVIDUAL PROPERTY NAME BASED ON ID / NAME COMBINATIONS
*	VIZ DEVICE NAME
*	VIZ DEVICE DESCRIPTION
*	VIZ COLOR
*	MQTT BROKER
*	MQTT USER
*	MQTT PWD
*	WIFI SSID
*	WIFI PWD
*/

//#define _wifi_ssid			"G6_3691"							// CHANGE THIS
//#define _wifi_password		"29622962"								// CHANGE THIS
#define _wifi_ssid			"Corelines_2"							// CHANGE THIS
#define _wifi_password		"fgj284ga"								// CHANGE THIS
#define _mqtt_broker		"test.mosquitto.org"//broker.shiftr.io"
#define _mqtt_port			1883
#define _mqtt_pub_topic		"outbox"	
#define _mqtt_sub_topic		"inbox"	

char *_mqtt_device_name = "ash_mezz_A_1";	// CHANGE THIS
char *_mqtt_device_description = "ASHMORE QLD AUST, MEZZANINE, #A1";	// CHANGE THIS
char _viz_color[8] = "#4D90FE";
char *_mqtt_username = "";	// CHANGE THIS IF USING CREDENTIALS
char *_mqtt_password = "";	// CHANGE THIS IF USING CREDENTIALS

bool _sent_device_info = false;
bool _in_config_mode = false;
NameValuePropViz _sensor_details[20];
byte _sensor_value_index = 0;

//---------------------------------Scope Declarations

void on_bus_received(char name[16], char value[16], byte address, Role role, PropViz prop_vizs);
void on_bus_complete();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
void mqtt_connect(const char* wifi_ssid, const char* wifi_password, const char* mqtt_broker, int mqtt_port);
void mqtt_create_subscriptions();
void mqtt_publish(char *root_topic, char *deviceName, char *endpoint, const char *payload);
void mqtt_reconnect();
void mqtt_subscribe(char *root_topic, char *deviceName, char *endpoint);

//---------------------------------Main

void setup()
{
	Serial.begin(9600);
	delay(5000);
	//ToDo: complete config/spiffs mode
	//Serial.println("IOT123 - SEND ANY KEY (WITH NEWLINE CHOSEN) TO ENTER CONFIG MODE, WITHIN 10 SECONDS");
	//delay(10000);// there will be a millis loop here that alows the user to set a flag that is used in the rest of setup and in loop - if flag, exit millis loop imediately
	Serial.println();
	Serial.println();
	if (!_in_config_mode){// possibly still get/show metadata
		// configure wifi and mqtt
		mqtt_connect(_wifi_ssid, _wifi_password, _mqtt_broker, _mqtt_port);
		Serial.println();
		Serial.println("AT THIS STAGE ONLY THE ADDRESSES ARE SET (FROM CONSTRUCTOR) - OTHERS ARE DEFAULTS");
		// start thge i2c bus rolling
		_assimilate_bus.print_metadata_details();
		_assimilate_bus.get_metadata();
		Serial.println();
		_assimilate_bus.print_metadata_details();
		Serial.println();
	}
	Serial.println("ASSIMILATE IOT NETWORK: NODE BUS");
}

void loop()
{
	if (!_in_config_mode) {
		mqtt_loop();
		_assimilate_bus.get_sensors(on_bus_received, on_bus_complete);
		delay(10000);
		//	delay(300000);
	} else {
		
	}
}

//---------------------------------I2C

void on_bus_received(char name[16], char value[16], byte address, Role role, PropViz prop_vizs){
	Serial.println("on_bus_received");
	if (!_sent_device_info)
	{
		// get the nv pairs until send of deviceInfo
		_sensor_details[_sensor_value_index].address = address;
		strcpy(_sensor_details[_sensor_value_index].name, name);
		strcpy(_sensor_details[_sensor_value_index].value, value);
		_sensor_details[_sensor_value_index].role = role;
		memcpy((void *)&_sensor_details[_sensor_value_index].prop_vizs, (void *)&prop_vizs, sizeof(prop_vizs));
		_sensor_value_index++;
		return;
	}
	Serial.println();
	mqtt_publish(_mqtt_pub_topic, _mqtt_device_name, name, value);
}

void on_bus_complete()
{
	//Serial.println("onBusComplete");
	if (_sent_device_info)
	{
		return;
	}
	String payload = _viz_json.build_device_info(_sensor_details, _sensor_value_index, _mqtt_device_name, _mqtt_device_description, _viz_color);
	Serial.println();
	Serial.println("SENDING");
	mqtt_publish(_mqtt_pub_topic, _mqtt_device_name, "deviceInfo", payload.c_str());
	Serial.println("SENT");
	// after sending the deviceInfo request the sensors again - happens first time only
	_sent_device_info = true;
	_assimilate_bus.get_sensors(on_bus_received, on_bus_complete);
	mqtt_create_subscriptions();
}


//---------------------------------MQTT

void mqtt_loop()
{
	if (!_client.connected()) {
		mqtt_reconnect();
	}
	_client.loop();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
	Serial.println("mqtt_callback");
	char payload_chars[50];
	Serial.println(topic);
	for (int i = 0; i < length; i++) {
		payload_chars[i] = (char)payload[i];
	}
//	Serial.println(payload_chars);
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(payload_chars);
	if (!root.success()) {
		Serial.println("parseObject() failed");
	}else
	{
		root.printTo(Serial);
		Serial.println();
	}
	// break up idx, value
	char *root_topic = strtok(topic, "/");
	char *device = strtok(NULL, "/");
	char *property = strtok(NULL, "/");
	byte address = -1;
	char card_type[16];
	for (int i = 0; i < sizeof(_sensor_details); i++)
	{
		if (strcmp(property, _sensor_details[i].name) == 0)
		{
			address = _sensor_details[i].address;
			strcpy(card_type, _sensor_details[i].prop_vizs.card_type);
			break;
		}
	}
	char *param;
	Serial.println(card_type);
	if (strcmp("toggle", card_type) == 0)
	{
		if (root["value"])
		{
			_assimilate_bus.send_to_actor(address, 2, "1");
		}else
		{
			_assimilate_bus.send_to_actor(address, 2, "0");
		}
		return;
	}
	if (strcmp("input", card_type) == 0)
	{
		_assimilate_bus.send_to_actor(address, 2, root["value"]);// limit to 16?
		return;
	}
	//if (strcmp("text", card_type) == 0)
	//{
	//	//na
	//}
	if (strcmp("slider", card_type) == 0)
	{
		_assimilate_bus.send_to_actor(address, 2, root["value"]);// limit to 16?
		return;
	}
	if (strcmp("button", card_type) == 0)
	{
		if (root["value"])
		{
			_assimilate_bus.send_to_actor(address, 2, "1");
		}
		else
		{
			_assimilate_bus.send_to_actor(address, 2, "0");
		}
		return;
	}
	//if (strcmp("chart-donut", card_type) == 0)
	//{
	//	//na
	//}
	//if (strcmp("chart-line", card_type) == 0)
	//{
	//	//na
	//}

}

void mqtt_subscribe(char *root_topic, char *deviceName, char *endpoint)
{
	Serial.println("mqtt_subscribe");
	char topic[127];
	sprintf(topic, "/%s/%s/%s", root_topic, deviceName, endpoint);
	Serial.println(topic);
	_client.subscribe(topic);
}

void mqtt_create_subscriptions()
{
	Serial.println("mqtt_create_subscriptions");
	if (_sensor_value_index == 0) // the sensors/actors have not been logged
	{
		return;
	}
	for (int i = 0; i < _sensor_value_index; i++)
	{
		if (strcmp("", _sensor_details[i].name) != 0){ // if assigned
			if (_sensor_details[i].role == ACTOR) // only actors
			{
				mqtt_subscribe(_mqtt_sub_topic, _mqtt_device_name, _sensor_details[i].name);
			}
		}
	}
}

void mqtt_reconnect() {
	Serial.println();
	Serial.println("mqtt_reconnect START");
	char lwt_topic[127];
	sprintf(lwt_topic, "/%s/%s/%s", _mqtt_pub_topic, _mqtt_device_name, "lwt");
	Serial.println(lwt_topic);
	while (!_client.connected()) {
		Serial.print("Attempting MQTT connection...");
		bool connect_success;
		if ((strcmp(_mqtt_username, "") == 0))// if not credentials
		{
			connect_success = _client.connect(_mqtt_device_name, lwt_topic, 0, false, "anything for last will and testament");
		}else // if credentials
		{
			connect_success = _client.connect(_mqtt_device_name, _mqtt_username, _mqtt_password, lwt_topic, 0, false, "anything for last will and testament");
		}
		if (connect_success) {
			Serial.println("connected");
			mqtt_create_subscriptions();
		} else {
			Serial.print("failed, rc=");
			Serial.print(_client.state());
			Serial.println(" try again in 5 seconds");
			delay(5000);
		}
	}
	Serial.println("mqtt_reconnect END");
}

void mqtt_connect(const char* wifi_ssid, const char* wifi_password, const char* mqtt_broker, int mqtt_port)
{
	Serial.println();
	Serial.println("mqtt_connect START");
	WiFi.mode(WIFI_STA);
	delay(10);
	WiFi.begin(wifi_ssid, wifi_password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	_client.setServer(mqtt_broker, mqtt_port);
	_client.setCallback(mqtt_callback);
	Serial.println();
	Serial.println("mqtt_connect END");
}

void mqtt_publish(char *root_topic, char *deviceName, char *endpoint, const char *payload)
{
	Serial.println("mqtt_publish");
	char topic[127];
	sprintf(topic, "/%s/%s/%s", root_topic, deviceName, endpoint);
	Serial.println(topic);
	_client.publish(topic, payload);
}