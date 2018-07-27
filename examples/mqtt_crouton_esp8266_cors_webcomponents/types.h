
#ifndef types_h
#define types_h

#include <ArduinoJson.h>
#define byte uint8_t

enum Role {
	SENSOR,
	ACTOR
};

struct SetupDeviceData {
	long sensor_interval;
	char ntp_server_name[64];
	byte time_zone;
	char wifi_ssid[32];
	char wifi_key[64];
	char mqtt_broker[64];
	int mqtt_port;
};

struct RuntimeDeviceData {
	char www_auth_username[10];
	char www_auth_password[10];
	char www_auth_exclude_files[320];
	char mqtt_username[16];
	char mqtt_password[16];
	char mqtt_device_name[32];
	char mqtt_device_description[64];
	char viz_color[8];// hex
};

struct NameValue {
	char name[16];
	char value[16];
};

struct PropertyDto
{
	byte slave_address;
	byte prop_index;
	char card_type[16];
	char name[16];
	char user_prop_name[64];
	char value[16];
	Role role;
	bool is_series;
	bool has_custom_card;
};

//struct MultiSeriesValue{
//	byte address;
//	NameValue values[5];
//	char *timestamp;	
//};

#endif