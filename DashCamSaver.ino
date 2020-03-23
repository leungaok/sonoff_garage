/*
  Shuts down the circuit when it's connected to known WIFI hotspots.
  Useful for Tesla's 3rd party installed dash cams, especially in parking mode.
  
  NOTE: Change YOUR_SSD and YOUR_SSD_PASSWORD to your home WIFI and upload to the Sonoff SV.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <chrono>

const char* ssid = "YOUR_SSD";
const char* wifiPassword = "YOUR_SSD_PASSWORD";
const char* otaPassword = wifiPassword;

const int gpio_13_led = 13;
const int gpio_12_relay = 12;

const int STATUS_CHECK_DELAY_MS = 5000; // Interval between checking different connection statuses.

const int DELAYED_RELAY_OFF_SECONDS = 300;  // Wait for how many seconds before switching off the relay.

ESP8266WebServer server(80);

String web_on_html = "<h1>SONOFF switch is ON</h1><p><a href=\"on\"><button>ON</button></a>&nbsp;<a href=\"off\"><button>OFF</button></a></p>";
String web_off_html = "<h1>SONOFF switch is OFF</h1><p><a href=\"on\"><button>ON</button></a>&nbsp;<a href=\"off\"><button>OFF</button></a></p>";

void setup(){  
  //  Init
  Serial.begin(115200); 
  delay(500);
  Serial.println("Booting up...");
  pinMode(gpio_13_led, OUTPUT);
  digitalWrite(gpio_13_led, HIGH);
  
  pinMode(gpio_12_relay, OUTPUT);
  digitalWrite(gpio_12_relay, HIGH);

  // Will automatically reconnect
  Serial.println("Connecting to wifi..");
  WiFi.begin(ssid, wifiPassword);

  Serial.println("Setting up Web Server for turning on/off switch from web browser..");
  setupWebServer();
  Serial.println("Setting up OTA server..");
  setupOTA();
  Serial.println("End setup");
}

std::chrono::time_point<std::chrono::system_clock> earliestConnectedTime;  // Earliest consecutive connected time
bool earliestConnectedTimeIsSet = false;  // flag needed because chrono lib can't be set to zero or Null.
bool isRelayOn = true;
 
void loop(){

  switch (WiFi.status()){
    case WL_CONNECTED:
      if(!earliestConnectedTimeIsSet){
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        earliestConnectedTime = std::chrono::system_clock::now();
        earliestConnectedTimeIsSet = true;
      }
      else if(double( std::chrono::duration<double> (std::chrono::system_clock::now() - earliestConnectedTime ).count()) > DELAYED_RELAY_OFF_SECONDS){
        if(isRelayOn){
          digitalWrite(gpio_12_relay, LOW);
          isRelayOn = false;
          Serial.println("Over " + String(DELAYED_RELAY_OFF_SECONDS / 60) + " mins connected to WIFI.  Relay turned off..");
        }
        else
          Serial.println("Over " + String(DELAYED_RELAY_OFF_SECONDS / 60) + " mins connected to WIFI.  Relay stays off..");
        Serial.println("Handling web client and OTA..");
        server.handleClient();
        ArduinoOTA.handle();
      }
      else{
        Serial.println("Connected in less than " + String(DELAYED_RELAY_OFF_SECONDS / 60) + " minutes ago.  Do nothing.");
      }
      break;
    case WL_NO_SSID_AVAIL:
      flashLED();
      Serial.println("." + WiFi.localIP());
      Serial.println(WiFi.status());
      earliestConnectedTimeIsSet = false;
      if(!isRelayOn){
        digitalWrite(gpio_12_relay, HIGH);
        isRelayOn = true;
        Serial.println("Relay turned on.");
      }
      break;
    case WL_CONNECT_FAILED:
      flashLED();
      Serial.println("Wrong WIFI password!  Leaving relay on.");
      if(!isRelayOn){
        digitalWrite(gpio_12_relay, HIGH);
        isRelayOn = true;
        Serial.println("Relay turned on.");
      }
      earliestConnectedTimeIsSet = false;
      
      delay(600000);
      break;
  }
  delay(STATUS_CHECK_DELAY_MS);
} 

void flashLED(){
      digitalWrite(gpio_13_led, LOW);
      delay(500);
      digitalWrite(gpio_13_led, HIGH);
      delay(500);
      digitalWrite(gpio_13_led, LOW);
      delay(500);
      digitalWrite(gpio_13_led, HIGH);
      delay(500);
}

void setupWebServer(){
    server.on("/", [](){
    if(digitalRead(gpio_12_relay)==HIGH) {
      server.send(200, "text/html", web_on_html);
    } else {
      server.send(200, "text/html", web_off_html);
    }
  });
  
  server.on("/on", [](){
    server.send(200, "text/html", web_on_html);
    digitalWrite(gpio_13_led, HIGH);
    digitalWrite(gpio_12_relay, HIGH);
    delay(1000);
  });
  
  server.on("/off", [](){
    server.send(200, "text/html", web_off_html);
    digitalWrite(gpio_13_led, HIGH);
    digitalWrite(gpio_12_relay, LOW);
    delay(1000); 
  });
  
  server.begin();
  Serial.println("Web Server ready..");
}

void setupOTA() {
    // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword(otaPassword);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
