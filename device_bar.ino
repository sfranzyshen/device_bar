#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <FS.h>

#define PIN            2
#define NUMPIXELS      27
#define START_BLK1     0 //0 - 14 (1-1 to START_BLK2-1) : Google
#define START_BLK2     15 //15 - 26 (START_BLK2-1 to NUMPIXELS-1) : Chico
#define USE_SERIAL     Serial

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

byte                   DNS_PORT = 53;
std::unique_ptr        <DNSServer>        dnsServer;
byte                   WEB_PORT = 80;
std::unique_ptr        <ESP8266WebServer> webServer;
byte                   WS_PORT = 81;
std::unique_ptr        <WebSocketsServer> wsServer;

uint8_t                wifi_status = 0;
uint8_t                wifi_mode = 0;
unsigned long          wifi_start = 0; 
long                   wifi_timeout = 0;
uint8_t                wifi_channel = 11;

//String                 id = "led_" + String(ESP.getChipId()).substring(4);
//const char*            wifi_name = id.c_str();
//const char*            wifi_ssid = "";
//const char*            wifi_password = "";

String                 wifi_name = "led_" + String(ESP.getChipId()).substring(4);
String                 wifi_ssid = "";
String                 wifi_password = "";

uint8_t                led_pattern = 0;
uint8_t                led_speed = 20;
uint8_t                led_red = 255;
uint8_t                led_green = 0;
uint8_t                led_blue = 18;

uint16_t               chico_cur_pos = 0;
uint32_t               chico_cur_color = 0;
long                   chico_wait = 0;
unsigned long          chico_previousMillis = 0;
bool                   chico_looping = false;
uint8_t                chico_mode = 0;
uint8_t                chico_state = 0;

bool loadConfig() {
   char input[32];
   File configFile = SPIFFS.open("/config.json", "r");
   if (!configFile) {
      USE_SERIAL.println("Failed to open config file");
      return false;
   }

   size_t size = configFile.size();
   if (size > 1024) {
      USE_SERIAL.println("Config file size is too large");
      return false;
   }

   std::unique_ptr<char[]> buf(new char[size]);
   configFile.readBytes(buf.get(), size);

   StaticJsonBuffer<200> jsonBuffer;
   JsonObject& json = jsonBuffer.parseObject(buf.get());

   if (!json.success()) {
      USE_SERIAL.println("Failed to parse config file");
      return false;
   }
   
   strcpy(input, json["wifi_name"]);
   wifi_name = String(input);
   strcpy(input, json["wifi_ssid"]);
   wifi_ssid = String(input);
   strcpy(input, json["wifi_password"]);
   wifi_password = String(input);
      
   //wifi_name = json["wifi_name"];
   //wifi_ssid = json["wifi_ssid"];
   //wifi_password = json["wifi_password"]; 
   
   led_pattern = json["led_pattern"];
   led_speed = json["led_speed"];
   led_red = json["led_red"];
   led_green = json["led_green"];
   led_blue = json["led_blue"];

   return true;
}

bool saveConfig() {
   StaticJsonBuffer<200> jsonBuffer;
   JsonObject& json = jsonBuffer.createObject();

   json["wifi_name"] = wifi_name;
   json["wifi_ssid"] = wifi_ssid;
   json["wifi_password"] = wifi_password;
   json["led_pattern"] = led_pattern;
   json["led_speed"] = led_speed;
   json["led_red"] = led_red;
   json["led_green"] = led_green;
   json["led_blue"] = led_blue;

   File configFile = SPIFFS.open("/config.json", "w");
   if (!configFile) {
      USE_SERIAL.println("Failed to open config file for writing");
      return false;
   }

   json.printTo(configFile);
   //json.printTo(Serial);
   USE_SERIAL.println("");
   return true;
}

void google_static() {
   pixels.setPixelColor(0, 5, 5, 180); // G
   pixels.setPixelColor(1, 5, 5, 180);
   pixels.setPixelColor(2, 5, 5, 180);
    
   pixels.setPixelColor(3, 255, 5, 5); // o
   pixels.setPixelColor(4, 255, 5, 5);

   pixels.setPixelColor(5, 255, 255, 5); // o
   pixels.setPixelColor(6, 255, 255, 5);
  
   pixels.setPixelColor(7, 5, 5, 180); // g
   pixels.setPixelColor(8, 5, 5, 180);
   pixels.setPixelColor(9, 5, 5, 180);

   pixels.setPixelColor(10, 5, 255, 5); // l
   pixels.setPixelColor(11, 5, 255, 5);  
   pixels.setPixelColor(12, 5, 255, 5);  

   pixels.setPixelColor(13, 255, 5, 5); // e
   pixels.setPixelColor(14, 255, 5, 5);  
   pixels.show();
}

uint32_t Wheel(byte WheelPos) {
   WheelPos = 255 - WheelPos;
   if(WheelPos < 85) {
      return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
   }
   if(WheelPos < 170) {
      WheelPos -= 85;
     return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
   }
   WheelPos -= 170;
   return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void chico_rainbow(long w) {
   chico_cur_pos = START_BLK2;
   chico_mode = 0;
   chico_wait = w;
   chico_looping = true;
}

void chico_rainbow_loop() { //Chico mode 1
   unsigned long currentMillis = millis();
   uint16_t i;
  
   if(currentMillis - chico_previousMillis >= chico_wait) {
      chico_previousMillis = currentMillis;   
      for(i = START_BLK2; i < pixels.numPixels(); i = i+2) {
         pixels.setPixelColor(i, Wheel((i+chico_cur_pos) & 255));
         pixels.setPixelColor(i+1, Wheel((i+chico_cur_pos) & 255));
      }
      pixels.show();

      if(chico_cur_pos++ >= 256)
         chico_cur_pos = 0;
  }
}

void chico_clear() { 
   for (int i = START_BLK2; i < pixels.numPixels(); i++) { 
      pixels.setPixelColor(i, 0);
      chico_cur_color = 0;
   }
   pixels.show();
}

void all_color(uint32_t c) { 
   for (int i = 0; i < pixels.numPixels(); i++) { 
      pixels.setPixelColor(i, c);
   }
   pixels.show();
}

void chico_colorWipe_loop() {
   unsigned long currentMillis = millis();
   if(currentMillis - chico_previousMillis >= chico_wait) {
      chico_previousMillis = currentMillis;   

      pixels.setPixelColor(chico_cur_pos, chico_cur_color);
      pixels.setPixelColor(chico_cur_pos + 1, chico_cur_color);
      pixels.show();

      chico_cur_pos = chico_cur_pos + 2;
      if(chico_cur_pos >= pixels.numPixels()) {
          chico_looping = false;
      }
   }
}

void chico_colorWipe(uint32_t c, long w) { //chico mode 2
   chico_cur_pos = START_BLK2;
   chico_cur_color = c;
   chico_mode = 2;
   chico_wait = w;
   chico_looping = true;
}

void chico_theaterChase(uint32_t c, long w) {
   chico_cur_pos = START_BLK2;
   chico_cur_color = c;
   chico_mode = 1;
   chico_wait = w;
   chico_state = 0;
   chico_looping = true;
}

void chico_theaterChase_loop() {
   unsigned long currentMillis = millis();
   if(currentMillis - chico_previousMillis >= chico_wait) {
      chico_previousMillis = currentMillis;

      if(chico_state == 0) {
         pixels.setPixelColor(chico_cur_pos + 0, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 1, chico_cur_color); 
       
         pixels.setPixelColor(chico_cur_pos + 2, 0);
         pixels.setPixelColor(chico_cur_pos + 3, 0);  

         pixels.setPixelColor(chico_cur_pos + 4, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 5, chico_cur_color); 
      
         pixels.setPixelColor(chico_cur_pos + 6, 0);
         pixels.setPixelColor(chico_cur_pos + 7, 0); 
       
         pixels.setPixelColor(chico_cur_pos + 8, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 9, chico_cur_color);  

         pixels.setPixelColor(chico_cur_pos + 10, 0);
         pixels.setPixelColor(chico_cur_pos + 11, 0); 
         pixels.show();
         chico_state = 1;
      } else {
         pixels.setPixelColor(chico_cur_pos + 0, 0);
         pixels.setPixelColor(chico_cur_pos + 1, 0); 
       
         pixels.setPixelColor(chico_cur_pos + 2, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 3, chico_cur_color);  

         pixels.setPixelColor(chico_cur_pos + 4, 0);
         pixels.setPixelColor(chico_cur_pos + 5, 0); 
      
         pixels.setPixelColor(chico_cur_pos + 6, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 7, chico_cur_color); 
       
         pixels.setPixelColor(chico_cur_pos + 8, 0);
         pixels.setPixelColor(chico_cur_pos + 9, 0);  

         pixels.setPixelColor(chico_cur_pos + 10, chico_cur_color);
         pixels.setPixelColor(chico_cur_pos + 11, chico_cur_color); 
         pixels.show();
         chico_state = 0;
      }
   }
}

void soft_reset() {
   pinMode(0,OUTPUT);
   digitalWrite(0,1);
   pinMode(2,OUTPUT);
   digitalWrite(2,1);
   ESP.restart();
} 

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
   switch(type) {
      case WStype_DISCONNECTED:
         USE_SERIAL.printf("[%u] Disconnected!\n", num);
      break;
   
      case WStype_CONNECTED: {
         IPAddress ip = wsServer->remoteIP(num);
         //USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
         wsServer->sendTXT(num, String("1" + wifi_name));
         wsServer->sendTXT(num, String("2" + wifi_ssid));
         wsServer->sendTXT(num, String("3" + wifi_password));
         wsServer->sendTXT(num, String("P" + String(led_pattern)));
         wsServer->sendTXT(num, String("S" + String(led_speed)));
         wsServer->sendTXT(num, String("R" + String(led_red)));
         wsServer->sendTXT(num, String("G" + String(led_green)));
         wsServer->sendTXT(num, String("B" + String(led_blue)));
      }
      break;

      case WStype_TEXT: { 
         //USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
         char PayLoad[lenght];
         if(payload[0] == '1') {
            // wifi_name
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            wifi_name = PayLoad;
            //USE_SERIAL.println(wifi_name);
         }
      
         if(payload[0] == '2') {
            // wifi_ssid
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            wifi_ssid = PayLoad;
            //USE_SERIAL.println(wifi_ssid);
         }  
               
         if(payload[0] == '3') {
            // wifi_password
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            wifi_password = PayLoad;
            //USE_SERIAL.println(wifi_password);
         }
      
         if(payload[0] == 'P') {
            // pattern
            led_pattern = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
            //USE_SERIAL.println(led_pattern);
            if(led_pattern == 0)
               chico_rainbow(led_speed);
            if(led_pattern == 2)
               chico_colorWipe(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
            if(led_pattern == 1)
               chico_theaterChase(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
         }
      
         if(payload[0] == 'S') {
            // speed
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            led_speed = (uint8_t) strtol(PayLoad, NULL, 10);
            //USE_SERIAL.println(led_speed);
            chico_wait = led_speed;
         }
      
         if(payload[0] == 'R') {
            // red
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            led_red = (uint8_t) strtol(PayLoad, NULL, 10);
            //USE_SERIAL.println(led_red);
            chico_cur_color = pixels.Color(led_red, led_green, led_blue);
            if(led_pattern == 2)
               chico_colorWipe(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
         }
         
         if(payload[0] == 'G') {
            // green
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            led_green = (uint8_t) strtol(PayLoad, NULL, 10);
            //USE_SERIAL.println(led_green);
            chico_cur_color = pixels.Color(led_red, led_green, led_blue);
            if(led_pattern == 2)
               chico_colorWipe(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
         }
         
         if(payload[0] == 'B') {
            // blue
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            led_blue = (uint8_t) strtol(PayLoad, NULL, 10);
            //USE_SERIAL.println(led_blue);
            chico_cur_color = pixels.Color(led_red, led_green, led_blue);
            if(led_pattern == 2) {
               chico_colorWipe(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
            }
         }
      
         if(payload[0] == 'U') {
            // update
            //USE_SERIAL.print("wifi_name: ");
            //USE_SERIAL.println(wifi_name);
            if(saveConfig()) {
               USE_SERIAL.println("Config file saved ... restarting");
               soft_reset();
            } else {
               USE_SERIAL.println("Config file save failed");
            }
         }
      }
      break;
   }
}

void setup() {
   USE_SERIAL.begin(115200);
   USE_SERIAL.println();

   USE_SERIAL.println("Starting LEDs ...");
   pixels.begin();
   //pixels.clear();
   pixels.show();
   google_static();
   
   if(SPIFFS.begin()) {
      USE_SERIAL.println("File system mounted");
   } else {
     USE_SERIAL.println("File system failed");
   }
   
   if (!loadConfig()) {
      USE_SERIAL.println("Failed to load config");
      chico_rainbow(led_speed);
   } else {
      USE_SERIAL.println("Config loaded"); //fixme to start correct pattern
      if(led_pattern == 0)
          chico_rainbow(led_speed);
      if(led_pattern == 2)
          chico_colorWipe(pixels.Color(led_red, led_green, led_blue), led_speed * 10);
      if(led_pattern == 1)
               chico_theaterChase(pixels.Color(led_red, led_green, led_blue), led_speed * 10);

      //chico_rainbow(led_speed);
      //all_color(pixels.Color(255, 255, 0));
      //chico_colorWipe(pixels.Color(255 , 0, 0), 200);
      //chico_theaterChase(pixels.Color(0 , 255, 0), 200);
   }
}

String getContentType(String filename) {
   if(webServer->hasArg("download")) return "application/octet-stream";
   else if(filename.endsWith(".htm")) return "text/html";
   else if(filename.endsWith(".html")) return "text/html";
   else if(filename.endsWith(".css")) return "text/css";
   else if(filename.endsWith(".js")) return "application/javascript";
   else if(filename.endsWith(".png")) return "image/png";
   else if(filename.endsWith(".gif")) return "image/gif";
   else if(filename.endsWith(".jpg")) return "image/jpeg";
   else if(filename.endsWith(".ico")) return "image/x-icon";
   else if(filename.endsWith(".wav")) return "audio/wav";
   else if(filename.endsWith(".mp3")) return "audio/mpeg";
   else if(filename.endsWith(".ogg")) return "audio/ogg";
   else if(filename.endsWith(".xml")) return "text/xml";
   else if(filename.endsWith(".pdf")) return "application/x-pdf";
   else if(filename.endsWith(".zip")) return "application/x-zip";
   else if(filename.endsWith(".gz")) return "application/x-gzip";
   else if(filename.endsWith(".json")) return "application/json";
   return "text/plain";
}

bool handleFileRead(String path) {
   USE_SERIAL.println("handleFileRead: " + path);
   if(path.endsWith("/")) path += "index.html";
      String contentType = getContentType(path);
   String pathWithGz = path + ".gz";
   if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
      if(SPIFFS.exists(pathWithGz))
         path += ".gz";
      File file = SPIFFS.open(path, "r");
      size_t sent = webServer->streamFile(file, contentType);
      file.close();
      return true;
   }
   return false;
}

boolean isMdns(String str) {
   if(str.substring(str.length() - 6) == ".local")
      return true;
   return false;
}

boolean isIp(String str) {
   for (int i = 0; i < str.length(); i++) {
      int c = str.charAt(i);
      if (c != '.' && (c < '0' || c > '9')) {
         return false;
      }
   }
   return true;
}

boolean captivePortal() {
   if ( !isIp(webServer->hostHeader()) && !isMdns(webServer->hostHeader()) ) { 
      USE_SERIAL.println("Request redirected to captive portal");
      webServer->sendHeader("Location", String("http://" + String(wifi_name) + ".local/" ), true);
      webServer->send ( 302, "text/plain", ""); 
      webServer->client().stop();
      return true;
   }
   return false;
}

void handleNotFound() {
   if (captivePortal()) { 
      return;
   }
   if(!handleFileRead(webServer->uri())) {
      String message = "File Not Found\n\n";
      message += "URI: ";
      message += webServer->uri();
      message += "\nMethod: ";
      message += ( webServer->method() == HTTP_GET ) ? "GET" : "POST";
      message += "\nArguments: ";
      message += webServer->args();
      message += "\n";

      for ( uint8_t i = 0; i < webServer->args(); i++ ) {
         message += " " + webServer->argName ( i ) + ": " + webServer->arg ( i ) + "\n";
      }
      webServer->send ( 404, "text/plain", message );
   }
}

void wifi_loop() {
   if(wifi_status != 3) {
      unsigned long currentMillis = millis();
      if(wifi_status == 4)
         return; // wifi disabled
      if(wifi_status == 0) { // try the default sta mode first
         USE_SERIAL.println("Station connecting ...");
         wifi_status = 1;
         wifi_start = millis();
         wifi_timeout = 20 * 1000; // 20 seconds
         WiFi.mode(WIFI_STA);
         if(wifi_ssid) {
            if(wifi_password) {
               WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
            } else {
               WiFi.begin(wifi_ssid.c_str());
            }
          } else {
            WiFi.begin();
      }
      return;
   }
   if(wifi_status == 1) { // check default sta status ...
      if(WiFi.status() != WL_CONNECTED) {
         if(currentMillis - wifi_start >= wifi_timeout) { // default sta connection timed out ... start default ap mode
            USE_SERIAL.println("Station timed out");
            wifi_status = 2;
            wifi_name = "led_" + String(ESP.getChipId()).substring(4);
            wifi_ssid = wifi_name;
            wifi_password = "";
            wifi_start = millis();
            wifi_timeout = 120 * 1000; // 2 minutes
            WiFi.mode(WIFI_AP); // start AP
            if(wifi_ssid == "")
               wifi_ssid = wifi_name;
            //USE_SERIAL.println(wifi_ssid);
            //USE_SERIAL.println(wifi_password);
            if(wifi_password != "") {
               if(WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str(), wifi_channel)) {
                  USE_SERIAL.println("AP started");
               } else {
                  USE_SERIAL.println("AP failed"); // AP failed ... shutdown wifi
                  wifi_status = 4;
                  WiFi.mode(WIFI_OFF);
                  return;
               }
            } else {
               if(WiFi.softAP(wifi_ssid.c_str(), NULL, wifi_channel)) {
                  USE_SERIAL.println("AP started");
               } else {
                  USE_SERIAL.println("AP failed"); // AP failed ... shutdown wifi
                  wifi_status = 4;
                  WiFi.mode(WIFI_OFF);
                  return;
               }               
            }
            dnsServer.reset(new DNSServer());        
            dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
            if(dnsServer->start(DNS_PORT, "*", WiFi.softAPIP())) { // captive dns portal
               USE_SERIAL.println("DNS captive portal started");
            } else {
               USE_SERIAL.println("DNS captive portal failed");
            }
            IPAddress ip = WiFi.softAPIP();;
            USE_SERIAL.print("IP Address: ");
            USE_SERIAL.println(ip);
            webServer.reset(new ESP8266WebServer(WEB_PORT));
            webServer->onNotFound(handleNotFound);
            webServer->begin();
            USE_SERIAL.println("HTTP server started");
            
            wsServer.reset(new WebSocketsServer(WS_PORT));
            wsServer->begin();
            USE_SERIAL.println("WebSocket server started");
            
            wsServer->onEvent(webSocketEvent);
            if(MDNS.begin(wifi_name.c_str())) {
               USE_SERIAL.println("MDNS responder started");
               MDNS.addService("http", "tcp", WEB_PORT);
               MDNS.addService("ws", "tcp", WS_PORT);
            } else {
               USE_SERIAL.println("MDNS responder failed");
            }
            return;
            } else {
              return; // tic-toc
            }
         } else { // sta is connected 
            // print your WiFi IP address:
            IPAddress ip = WiFi.localIP();
            USE_SERIAL.print("IP Address: ");
            USE_SERIAL.println(ip);

            webServer.reset(new ESP8266WebServer(WEB_PORT));
            webServer->onNotFound(handleNotFound);
            webServer->begin();
            USE_SERIAL.println("HTTP server started");
        
            wsServer.reset(new WebSocketsServer(WS_PORT));
            wsServer->begin();
            USE_SERIAL.println("WebSocket server started");
            wsServer->onEvent(webSocketEvent);
       
            if(MDNS.begin(wifi_name.c_str())) {
               USE_SERIAL.println("MDNS responder started");
               MDNS.addService("http", "tcp", WEB_PORT);
               MDNS.addService("ws", "tcp", WS_PORT);
            } else {
               USE_SERIAL.println("MDNS responder failed");
            }
            wifi_mode = 1;
            wifi_status = 3;
            return;
         }
      }
      if(wifi_status == 2) { // AP mode
         if(WiFi.softAPgetStationNum() == 0) { // no connections to AP
            if(currentMillis - wifi_start >= wifi_timeout) { // AP connection timed out 
               USE_SERIAL.println("AP timed out "); // AP timed out ... shutdown wifi
               wifi_status = 4;
               WiFi.mode(WIFI_OFF);
               return;
            }
         } else {
            wifi_status = 3; // we have an AP connection
         }
      }
      return;
   } // wifi is connected ...
   if(wifi_mode != 1) 
      dnsServer->processNextRequest();
   webServer->handleClient();
   wsServer->loop();
}

void google_loop() {
   // fixme
   return;
}

void chico_loop() {
   if(chico_looping) {
      if(chico_mode == 0)
         chico_rainbow_loop();
      if(chico_mode == 2)
         chico_colorWipe_loop();
      if(chico_mode == 1)
         chico_theaterChase_loop();
   }
}

void loop() {
   chico_loop();
   google_loop();
   wifi_loop();
   //yield();
}




