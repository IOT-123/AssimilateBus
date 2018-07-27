#include "types.h"
#include "VizJson.h"
#include "assimilate_bus.h"
#include "debug.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // set MQTT_MAX_PACKET_SIZE to ~3000 (or your needs for deviceInfo json)
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <neotimer.h>
//---------------------------------MEMORY DECLARATIONS
//-------------------------------------------------- defines
#define DBG_OUTPUT_FLAG 2 //0,1,2 MINIMUMUM,RELEASE,FULL
#define _mqtt_pub_topic    "outbox"  
#define _mqtt_sub_topic   "inbox" 
//-------------------------------------------------- class objects
Debug _debug(DBG_OUTPUT_FLAG);
AssimilateBus _assimilate_bus;
VizJson _viz_json;
Config _config_data;
WiFiClient _esp_client;
PubSubClient _client(_esp_client);
WiFiUDP Udp;
ESP8266WebServer _server(80);
Neotimer _timer_sensor = Neotimer(5000);
//-------------------------------------------------- data structs / variable
RuntimeDeviceData _runtime_device_data;
PropertyDto _dto_props[50]; // max 10 slaves x max 5 properties
//-------------------------------------------------- control flow
volatile bool _sent_device_info = false;
byte _dto_props_index = 0;
bool _fatal_error = false;

//---------------------------------FUNCTION SCOPE DECLARATIONS
//-------------------------------------------------- static_i2c_callbacks.ino
void on_bus_received(byte slave_address, byte prop_index, Role role, char name[16], char value[16]);
void on_bus_complete();
//-------------------------------------------------- static_mqtt.ino
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
int8_t mqtt_get_topic_index(char* topic);
void mqtt_init(const char* wifi_ssid, const char* wifi_password, const char* mqtt_broker, int mqtt_port);
void mqtt_create_subscriptions();
void mqtt_publish(char *root_topic, char *deviceName, char *endpoint, const char *payload);
bool mqtt_ensure_connect();
void mqtt_subscribe(char *root_topic, char *deviceName, char *endpoint);
void i2c_set_and_get(byte address, byte code, const char *param);
//-------------------------------------------------- static_server.ino
String server_content_type_get(String filename);
bool server_path_in_auth_exclusion(String path);
bool server_auth_read(String path);
bool server_file_read(String path);
void server_file_upload();
void server_file_delete();
void server_file_create();
void server_file_list();
void  server_init();
void time_services_init(char *ntp_server_name, byte time_zone);
time_t get_ntp_time();
void send_ntp_packet(IPAddress &address);
char *time_stamp_get();
//-------------------------------------------------- static_utility.ino
String spiffs_file_list_build(String path);
void report_deserialize_error();
void report_spiffs_error();
bool check_fatal_error();
bool get_json_card_type(byte slave_address, byte prop_index, char *card_type);
bool get_struct_card_type(byte slave_address, byte prop_index, char *card_type);
bool get_json_is_series(byte slave_address, byte prop_index);


//---------------------------------MAIN

void setup(){
	DBG_OUTPUT_PORT.begin(115200);
	SetupDeviceData device_data;
	Serial.println(); Serial.println(); // margin for console rubbish
	delay(5000);
	if (DBG_OUTPUT_FLAG == 2)DBG_OUTPUT_PORT.setDebugOutput(true);
	_debug.out_fla(F("setup"), true, 2);
	if (SPIFFS.begin()){ // get required config
		_debug.out_str(spiffs_file_list_build("/"), true, 2);
		if (!_config_data.get_device_data(device_data, _runtime_device_data)){
			report_deserialize_error();
			return;
		}
	}else{
		report_spiffs_error();
		return;
	}
	_timer_sensor.set(device_data.sensor_interval);
	mqtt_init(device_data.wifi_ssid, device_data.wifi_key, device_data.mqtt_broker, device_data.mqtt_port);
	time_services_init(device_data.ntp_server_name, device_data.time_zone);
	server_init();
	_assimilate_bus.get_metadata();
	_assimilate_bus.print_metadata_details();
	// kick off the metadata collection - needs sensor property (names) to complete
	mqtt_ensure_connect(); 
	_assimilate_bus.get_properties(on_bus_received, on_bus_complete);
	_timer_sensor.reset(); // can ellapse noticable time till this point
}

void loop(){
	if (!check_fatal_error()) return;
	mqtt_loop();
	_server.handleClient();
	if(_timer_sensor.repeat()){
		_assimilate_bus.get_properties(on_bus_received, on_bus_complete);
	}
}
