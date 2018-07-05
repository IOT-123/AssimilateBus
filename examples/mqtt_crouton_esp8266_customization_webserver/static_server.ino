#define NTP_LOCAL_PORT 8888                               // local port to listen for UDP packets
#define NTP_PACKET_SIZE 48                             // NTP time is in the first 48 bytes of message
byte _packet_buffer[NTP_PACKET_SIZE];                  //buffer to hold incoming & outgoing packets
char _ntp_server_name[64];
byte _time_zone;
long _time_substitute = 0;
File _fs_upload_file;

String server_content_type_get(String filename){
  if(_server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool server_path_in_auth_exclusion(String path){
  const char s[2] = ";";
  char *token;
   /* get the first token */
   token = strtok(_runtime_device_data.www_auth_exclude_files, s);
   /* walk through other tokens */
   while( token != NULL ) {
    if (path = String(token)){
      return true;
    }
    token = strtok(NULL, s);
   }
   return false;
}

bool server_auth_read(String path){
    if ((strcmp(_runtime_device_data.www_auth_username, "") == 0))// if not credentials
    {
      return true;
    }else // if credentials
    {
      if (server_path_in_auth_exclusion(path)){
        return true;
      }
      if(!_server.authenticate(_runtime_device_data.www_auth_username, _runtime_device_data.www_auth_password)){
        return false;
      }
      return true;
    }
}

bool server_file_read(String path){
  _debug.out_fla(F("[HTTP READ] "), false, 1); _debug.out_str(path, true, 1);
  if (!server_auth_read(path)){
    _server.requestAuthentication();
  }
  if(path.endsWith("/")) path += "editor/edit.htm";
  String contentType = server_content_type_get(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    _server.streamFile(file, contentType);
    file.close();
    return true;
  }
  Serial.println(F("[HTTP READ] FAIL"));
  return false;
}

void server_file_upload(){
  if(_server.uri() != "/edit") return;
  HTTPUpload& upload = _server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    _debug.out_fla(F("[HTTP UPLOAD] "), false, 1); _debug.out_str(filename, true, 1);
	_fs_upload_file = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
	if(_fs_upload_file)
	_fs_upload_file.write(upload.buf, upload.currentSize);
	  } else if(upload.status == UPLOAD_FILE_END){
		if(_fs_upload_file)
			_fs_upload_file.close();
	  }
}

void server_file_delete(){
  if(_server.args() == 0) return _server.send(500, "text/plain", "BAD ARGS");
  String path = _server.arg(0);
  _debug.out_fla(F("[HTTP DELETE] "), false, 1); _debug.out_str(path, true, 1);
  if(path == "/")
    return _server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return _server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  _server.send(200, "text/plain", "");
  path = String();
}

void server_file_create(){
  if(_server.args() == 0)
    return _server.send(500, "text/plain", "BAD ARGS");
  String path = _server.arg(0);
  _debug.out_fla(F("[HTTP CREATE] "), false, 1); _debug.out_str(path, true, 1);
  if(path == "/")
    return _server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return _server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return _server.send(500, "text/plain", "CREATE FAILED");
  _server.send(200, "text/plain", "");
  path = String();
}

void server_file_list() {
  if(!_server.hasArg("dir")) {_server.send(500, "text/plain", "BAD ARGS"); return;}
  String path = _server.arg("dir");
  _debug.out_fla(F("[HTTP LIST] "), false, 1); _debug.out_str(path, true, 1);
  String output = spiffs_file_list_build(path);
  _server.send(200, "text/json", output);
}

void  server_init(){
  //list directory
  _server.on("/list", HTTP_GET, server_file_list);
  //load editor
  _server.on("/", [](){
    if(!server_file_read("/editor/edit.htm")) _server.send(404, "text/plain", "FileNotFound");
  });
  _server.on("/edit", HTTP_GET, [](){
    if(!server_file_read("/editor/edit.htm")) _server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  _server.on("/edit", HTTP_PUT, server_file_create);
  //delete file
  _server.on("/edit", HTTP_DELETE, server_file_delete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  _server.on("/edit", HTTP_POST, [](){ _server.send(200, "text/plain", ""); }, server_file_upload);
  //called when the url is not defined here
  //use it to load content from SPIFFS
  _server.onNotFound([](){
    if(!server_file_read(_server.uri()))
      _server.send(404, "text/plain", "FileNotFound");
  });
  //get heap status, analog input value and all GPIO statuses in one json call
  _server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    _server.send(200, "text/json", json);
    json = String();
  });
  _server.begin();
  Serial.print(F("[HTTP SERVER] http://"));
  Serial.print(WiFi.localIP());
  Serial.println(F("/"));
}

void time_services_init(char *ntp_server_name, byte time_zone){
  strcpy(_ntp_server_name, ntp_server_name);
  _time_zone = time_zone;
  Udp.begin(NTP_LOCAL_PORT);
  setSyncProvider(get_ntp_time);//setSyncInterval(300);}
}

time_t get_ntp_time(){
  IPAddress ntp_server_ip; // NTP server's ip address: get a random server from the pool
  WiFi.hostByName(_ntp_server_name, ntp_server_ip);
  send_ntp_packet(ntp_server_ip);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(_packet_buffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)_packet_buffer[40] << 24;
      secsSince1900 |= (unsigned long)_packet_buffer[41] << 16;
      secsSince1900 |= (unsigned long)_packet_buffer[42] << 8;
      secsSince1900 |= (unsigned long)_packet_buffer[43];
      _debug.out_fla(F("[NTP BEGIN] "), false, 1); _debug.out_char(_ntp_server_name, true, 1);
      return secsSince1900 - 2208988800UL + _time_zone * SECS_PER_HOUR;
    }
  }
  Serial.print(F("[NTP FAIL] "));
  Serial.println(_ntp_server_name);
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void send_ntp_packet(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(_packet_buffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  _packet_buffer[0] = 0b11100011;   // LI, Version, Mode
  _packet_buffer[1] = 0;     // Stratum, or type of clock
  _packet_buffer[2] = 6;     // Polling Interval
  _packet_buffer[3] = 0xEC;  // Peer Clock Precision
                 // 8 bytes of zero for Root Delay & Root Dispersion
  _packet_buffer[12] = 49;
  _packet_buffer[13] = 0x4E;
  _packet_buffer[14] = 49;
  _packet_buffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(_packet_buffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// if timeserver issues return incremented value
char *time_stamp_get() {
  char result[10];
  if (timeStatus() != timeNotSet) {
    int hours = hour();
    int minutes = minute();
    sprintf(result, "%02d:%02d", hours, minutes);
  }
  else {
    _time_substitute++;
    if (2147483646 < _time_substitute) {
      _time_substitute = 0;
    }
    itoa(_time_substitute, result, 10);
  }
  return result;
}
