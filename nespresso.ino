#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <WiFiClient.h> // Not needed if no need to get WiFi.localIP
#include <DNSServer.h>           //needed for WiFiManager
#include <ESP8266WebServer.h>    //needed for this program and for WiFiManager
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>

#include "Credentials.h"

const char* software_name = "nespresso";
const char* software_version = "0.2.1";

const char* www_username = "denouche";
const char* www_password = "denouche";

// Put here the fingerprint of above domain name HTTPS certificate
// iot.leveugle.net TLS SHA-1 fingerprint, issued on 2017-09-05
String API_HTTPS_FINGERPRINT = "B9 8C A0 87 A8 94 C0 11 C6 45 53 30 8D F8 44 48 DA 60 08 C5";

ESP8266WebServer server(80);

// Declare pin number here
const int GPIO_PIN_5_LONG = 5;
const int GPIO_PIN_12_SHORT = 12;

byte mac[6];
boolean locked = false;

void turnOnAndWait() {
    digitalWrite(GPIO_PIN_12_SHORT, HIGH);
    delay(300);
    digitalWrite(GPIO_PIN_12_SHORT, LOW);
    delay(30000); // wait for 30 seconds until the machine is ready
}

void turnOff() {
    digitalWrite(GPIO_PIN_12_SHORT, HIGH);
    digitalWrite(GPIO_PIN_5_LONG, HIGH);
    delay(500);
    digitalWrite(GPIO_PIN_12_SHORT, LOW);
    digitalWrite(GPIO_PIN_5_LONG, LOW);
}

void handleCoffeeLong() {
    if(!server.authenticate(www_username, www_password)) {
        return server.requestAuthentication();
    }

    if(locked) {
      server.send(409);
    }
    else {
      locked = true;
      server.send(204);

      turnOnAndWait();

      digitalWrite(GPIO_PIN_5_LONG, HIGH);
      delay(300);
      digitalWrite(GPIO_PIN_5_LONG, LOW);

      delay(120000); // TODO vérifier le temps nécéssaire pour faire un café, au maximum
      turnOff();
      locked = false;
    }
}

void handleCoffeeShort() {
    if(!server.authenticate(www_username, www_password)) {
        return server.requestAuthentication();
    }

    if(locked) {
      server.send(409);
    }
    else {
      locked = true;
      server.send(204);

      turnOnAndWait();

      digitalWrite(GPIO_PIN_12_SHORT, HIGH);
      delay(300);
      digitalWrite(GPIO_PIN_12_SHORT, LOW);

      delay(90000); // TODO vérifier le temps nécéssaire pour faire un café, au maximum
      turnOff();
      locked = false;
    }
}

void handleNotFound() {
	  server.send(404);
}

String info() {
    String jsonString = getInformations();
    Serial.println(jsonString);
    return jsonString;
}

void handleInfo() {
    String jsonString = info();
    server.send(200, "application/json", jsonString);
}

void update() {
    String macString = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
    t_httpUpdate_return ret = ESPhttpUpdate.update(API_HOSTNAME, 80, API_BASE_URL + "/download?mac=" + macString);
    Serial.println(ret);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.println("HTTP_UPDATE_FAILD Error code " + String(ESPhttpUpdate.getLastError()));
            Serial.println("HTTP_UPDATE_FAILD Error " + String(ESPhttpUpdate.getLastErrorString().c_str()));
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            break;
    }
}

void handleUpdate() {
    update();
    server.send(204);
}

void handleDisconnect() {
    server.send(204);
    delay(200);
    WiFi.disconnect();
}

String getInformations() {
    const size_t bufferSize = JSON_OBJECT_SIZE(6) + 250;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.createObject();

    root["application"] = software_name;
    root["version"] = software_version;
    root["mac"] = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
    root["ssid"] = WiFi.SSID();
    root["ip"] = String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
    root["plateform"] = "esp8266";

    String jsonString;
    root.printTo(jsonString);
    return jsonString;
}

void informConnexionDone() {
    String jsonString = getInformations();

    HTTPClient http;
    http.begin(API_PROTOCOL + API_HOSTNAME + API_BASE_URL + "/devices/register", API_HTTPS_FINGERPRINT);
    http.addHeader("Authorization", API_AUTHORIZATION);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonString);
    http.end();
}

void setup ( void ) {
    Serial.begin(115200);
    Serial.println("setup - begin");

    // init pin state here
    pinMode(GPIO_PIN_5_LONG, OUTPUT);
    digitalWrite(GPIO_PIN_5_LONG, LOW);  // by default, the start switch is in parallel with this optocouper, so transistor should be OFF by default. Pull-down resistor.

    pinMode(GPIO_PIN_12_SHORT, OUTPUT);
    digitalWrite(GPIO_PIN_12_SHORT, LOW);  // by default, the start switch is in parallel with this optocouper, so transistor should be OFF by default. Pull-down resistor.

    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");

    WiFi.macAddress(mac);

    Serial.println("");
    // Wait for connection
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected!\n");

    info();
    informConnexionDone();
    update();
    info();

    server.on("/info", handleInfo);
    server.on("/status", handleInfo);

    server.on("/short", handleCoffeeShort);
    server.on("/long", handleCoffeeLong);

    server.on("/update", handleUpdate);
    server.on("/disconnect", handleDisconnect);

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}


