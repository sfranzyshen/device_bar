#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <FS.h>

#define USE_SERIAL     Serial

byte                   DNS_PORT = 53;
std::unique_ptr        <DNSServer>        dnsServer;
byte                   WEB_PORT = 80;
std::unique_ptr        <ESP8266WebServer> webServer;
byte                   WS_PORT = 81;
std::unique_ptr        <WebSocketsServer> wsServer;

uint8_t                wifi_status = 0;
uint8_t                wifi_dns = 0;
uint8_t                wifi_mode = 0;
unsigned long          wifi_start = 0; 
long                   wifi_timeout = 0;

uint8_t                wifi_channel = 11;
String                 wifi_name = "led_" + String(ESP.getChipId()).substring(4);
String                 wifi_ssid = "";
String                 wifi_password = "";

bool                   channel_1  = false;
uint8_t                channel_1_high = 0;
bool                   channel_6  = false;
uint8_t                channel_6_high = 0;
bool                   channel_11 = false;
uint8_t                channel_11_high = 0;
uint8_t                channel_open = 0;
uint8_t                channel_weakest = 0;
bool                   scanning = false;
  
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
   strcpy(input, json["wifi_channel"]);
   wifi_channel = String(input).toInt();
   strcpy(input, json["wifi_ssid"]);
   wifi_ssid = String(input);
   strcpy(input, json["wifi_password"]);
   wifi_password = String(input);
      
   return true;
}

bool saveConfig() {
   StaticJsonBuffer<200> jsonBuffer;
   JsonObject& json = jsonBuffer.createObject();

   json["wifi_name"] = wifi_name;
   json["wifi_channel"] = wifi_channel;
   json["wifi_ssid"] = wifi_ssid;
   json["wifi_password"] = wifi_password;

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
	       String Send = String("1"+ wifi_name);
         //wsServer->sendTXT(num, String("1" + wifi_name));
	       wsServer->sendTXT(num, Send);
	       Send = String("2"+ wifi_ssid);
         //wsServer->sendTXT(num, String("2" + wifi_ssid));
         //wsServer->sendTXT(num, String("3" + wifi_password));
	       wsServer->sendTXT(num, Send);
	       Send = String("3"+ wifi_password);
         wsServer->sendTXT(num, Send);
         Send = String("4"+ wifi_channel);
         wsServer->sendTXT(num, Send);
      }
      break;

      case WStype_TEXT: { 
         //USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
         char PayLoad[lenght];
         String Chan;
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
               
         if(payload[0] == '4') {
            // wifi_password
            for(int i=0; i < lenght -1; i++) {
               PayLoad[i] = payload[i+1];
            }
            PayLoad[lenght - 1] = '\0';
            Chan = PayLoad;
            wifi_channel = Chan.toInt();
            //USE_SERIAL.println(wifi_channel);
         }               

         if(payload[0] == 'U') {
            // update
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

   if(SPIFFS.begin()) {
      USE_SERIAL.println("File system mounted");
   } else {
     USE_SERIAL.println("File system failed");
   }
   if (!loadConfig()) {
      USE_SERIAL.println("Failed to load config");
   } else {
      USE_SERIAL.println("Config loaded"); 
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
               //WiFi.mode(WIFI_STA);
               WiFi.disconnect();
               delay(100);
               wifi_status = 2; 
               scanning = true;
               WiFi.scanNetworks(true, true);
               Serial.println("scan started");
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
            wifi_dns = 1;
            wifi_status = 3;
            return;
         }
      }

      if(wifi_status == 2) { // AP mode
         if(scanning) {
            int n = WiFi.scanComplete();
            if(n == -1) {
               return; // still scanning
            }
            if(n == -2) {
               scanning = false; // problem ... reset fixme
               Serial.println("scan failed");
               wifi_status = 4;
               WiFi.mode(WIFI_OFF);
               return;
            }
            Serial.println("scan done");
            scanning = false;
            if(n == 0) {
               channel_open = 1; // no networks found use channel 1
            } else {
               for (int i = 0; i < n; ++i) {
                  if(WiFi.channel(i) == 1) {
                     channel_1 = true;
                     if( (abs(WiFi.RSSI(i)) < channel_1_high) || channel_1_high == 0) {
                        channel_1_high = abs(WiFi.RSSI(i));
                     }
                  }

                  if(WiFi.channel(i) == 6) {
                     channel_6 = true;
                     if( (abs(WiFi.RSSI(i)) < channel_6_high) || channel_6_high == 0) {
                        channel_6_high = abs(WiFi.RSSI(i));
                     }
                  }
      
                  if(WiFi.channel(i) == 11) {
                     channel_11 = true;
                     if( (abs(WiFi.RSSI(i)) < channel_11_high) || channel_11_high == 0) {
                        channel_11_high = abs(WiFi.RSSI(i));
                     }
                  }
               }
            }
  
            if(!channel_11) {
               channel_open = 11;
            }
    
            if(!channel_6) {
               channel_open = 6;
            }
        
            if(!channel_1) {
               channel_open = 1;
            }

            if(channel_open == 0) {
               Serial.print("Using weakest channel: ");
    
               if(channel_1_high > channel_6_high && channel_1_high > channel_11_high) {
                  channel_weakest = 1;
               } 
    
               if(channel_6_high > channel_1_high && channel_6_high > channel_11_high) {
                  channel_weakest = 6;
               } 
    
               if(channel_11_high > channel_6_high && channel_11_high > channel_1_high) {
                  channel_weakest = 11;
               }
            
               Serial.println(channel_weakest);
               wifi_channel = channel_weakest;
            } else {
               Serial.print("Using open channel: ");
               Serial.println(channel_open);
               wifi_channel = channel_open;
            } // channel set
            
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
         } 

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
   if(wifi_dns != 1) 
      dnsServer->processNextRequest();
   webServer->handleClient();
   wsServer->loop();
}


void loop() {
   wifi_loop();
   //yield();
}




