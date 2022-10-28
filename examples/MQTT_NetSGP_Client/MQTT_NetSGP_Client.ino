
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "AsyncNETSGPClient.h"
#include <esp_wifi.h>

const char        CompileDate[] = __DATE__ " " __TIME__;
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

char              MqttServer[] = "my.mqttserver.com"; // address of your MQTT Server
unsigned int      MqttPort = 1883;            // MQTT port number. default is 1883
const char        *Mqtt_User = "mqqt_client";
const char        *Mqtt_Password = "123456";

const char        *ssid = "WLAN";
const char        *pass = "12345678";

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

constexpr const uint8_t PIN_LC12S_SET = 4; /// Programming enable pin of RF module
constexpr const uint8_t PIN_LC12S_RX = 16; /// RX pin of ESP connect to TX of RF module
constexpr const uint8_t PIN_LC12S_TX = 17; /// TX pin of ESP connect to RX of RF module

char               macname[12];               // MAC Name      eg ESP_AABBCC

String                      payload = "";
volatile boolean            publish_payload = 0;
volatile bool               publish_esp_info = false;
volatile bool               publish_ip_info = false;
volatile bool               publish_getInverters = false;
volatile bool               publish_Inverter = false;

WiFiClient net;

PubSubClient mqtt_client(net);
//AsyncNETSGPClient netsgp_client(Serial2, PIN_LC12S_SET); // Defaults to fetch status every 2 seconds
AsyncNETSGPClient *netsgp_client;

volatile NETSGPClient::InverterStatus vInverter;

uint32_t lastSendMillis = 0;
uint32_t lastWifiMillis = 0;
uint32_t lastMQTTMillis = 0;


// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

void callback(char* topic, byte* msg_payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)msg_payload[i]);
  }
  Serial.println();

  msg_payload[length] = '\0';
  String _message = String((char*)msg_payload);

  char *token = strtok(topic, "/");  // initialize token
  if (token != NULL) token = strtok(NULL, "/");          // now token = 2nd token
  if (token != NULL) token = strtok(NULL, "/");          // now token = 2nd token

  if (!strncmp(token, "info", 4)) {
    Serial.println("info");
    publish_esp_info = true;
  }

  if (!strncmp(token, "registerInverter", 16)) {
    uint32_t inverterID = strtoul(_message.c_str(), NULL, 16);

    char buff[50];
    sprintf(buff, "{\"registerInverter\":\"%08lX\"}", inverterID);
    payload = String(buff);
    publish_payload = true;
    publish_getInverters = true;
    netsgp_client->registerInverter(inverterID);
  }

  if (!strncmp(token, "deregisterInverter", 18)) {
    uint32_t inverterID = strtoul(_message.c_str(), NULL, 16);

    char buff[50];
    sprintf(buff, "{\"deregisterInverter\":\"%08lX\"}", inverterID);
    payload = String(buff);
    publish_payload = true;
    publish_getInverters = true;
    netsgp_client->deregisterInverter(inverterID);
  }

  if (!strncmp(token, "rebootInverter", 14)) {
    uint32_t inverterID = strtoul(_message.c_str(), NULL, 16);

    char buff[50];
    sprintf(buff, "{\"rebootInverter\":\"%08lX\"}", inverterID);
    payload = String(buff);
    publish_payload = true;
    netsgp_client->reboot(inverterID);
  }

  if (!strncmp(token, "activateInverter", 16)) {
    char *ptr;
    uint32_t inverterID = strtoul(_message.c_str(), &ptr, 16);
    uint32_t value = strtoul(ptr, NULL, 10);

    Serial.printf("value: %d\n", value);

    char buff[50];
    sprintf(buff, "{\"activateInverter\":\"%08lX %d\"}", inverterID, value);
    payload = String(buff);
    publish_payload = true;
    netsgp_client->activate(inverterID, value);
  }

  if (!strncmp(token, "setPowerGrade", 13)) {
    char *ptr;
    uint32_t inverterID = strtoul(_message.c_str(), &ptr, 16);
    uint32_t value = strtoul(ptr, NULL, 10);
    char buff[50];
    sprintf(buff, "{\"setPowerGrade\":\"%08lX %d\"}", inverterID, value);
    payload = String(buff);
    publish_payload = true;
    netsgp_client->setPowerGrade(inverterID, (NETSGPClient::PowerGrade) value);
  }

  if (!strncmp(token, "getInverters", 12)) {
    publish_getInverters = true;
    Serial.println("getInverters");
  }

  if (!strncmp(token, "clearInverters", 14)) {
    netsgp_client->clearInverters();
    publish_getInverters = true;
    Serial.println("clearInverters");
  }

  if (!strncmp(token, "saveInverters", 14)) {
    payload = "{\"saveInverters\":\"not implemented yet\"}";
    publish_payload = true;
    Serial.println("saveInverters not implemented yet");
  }

  if (!strncmp(token, "restart", 7)) {
    ESP.restart();
    Serial.println("restart");
  }

  if (!strncmp(token, "InverterInterval", 16)) {
    uint32_t ms = strtoul(_message.c_str(), NULL, 10);
    char buff[50];
    if (ms < 333) ms = 1000;
    netsgp_client->mIntervalMS = ms;
    sprintf(buff, "{\"InverterInterval\":\"%d\"}", ms);
    payload = String(buff);
    publish_payload = true;
    Serial.println(buff);
  }

}


// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void onInverterStatus(const AsyncNETSGPClient::InverterStatus& status)
{
  Serial.println("*********************************************");
  Serial.println("Received Inverter Status");
  Serial.print("Device: ");
  Serial.println(status.deviceID, HEX);

  Serial.println("Status: " + String(status.state));
  //Serial.printf("Status: %08b\n", status.state);
  //Serial.printf("Status: %s\n", itoa(15, str, 2));
  Serial.println(status.state, BIN);

  Serial.println("DC_Voltage: " + String(status.dcVoltage) + "V");
  Serial.println("DC_Current: " + String(status.dcCurrent) + "A");
  Serial.println("AC_Voltage: " + String(status.acVoltage) + "V");
  Serial.println("AC_Current: " + String(status.acCurrent) + "A");
  Serial.println("Power gen total: " + String(status.totalGeneratedPower));
  Serial.println("Temperature: " + String(status.temperature));

  if (!publish_Inverter) {
    memcpy((void*)&vInverter, &status, sizeof(vInverter));
    publish_Inverter = true;
  }
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  while (!Serial);
  delay(200);

  Serial.println(F("VND MQTT Async NetSGP Client"));

  { // ESP Standard Hostname generieren
    uint8_t locMAC[6];
    WiFi.macAddress(locMAC);
    sprintf(macname, "ESP_%02X%02X%02X", locMAC[3], locMAC[4], locMAC[5]);
    Serial.println("ME " + String(macname));
  }

  Serial.println(F("New AsyncNETSGPClient"));
  netsgp_client = new AsyncNETSGPClient(Serial2, PIN_LC12S_SET); // Defaults to fetch status every 2 seconds

  Serial2.begin(9600, SERIAL_8N1, PIN_LC12S_RX, PIN_LC12S_TX);

  // Make sure the RF module is set to the correct settings
  if (!netsgp_client->setDefaultRFSettings())
  {
    Serial.println(F("ERROR! Could not set RF module to default settings"));
  }
  netsgp_client->setStatusCallback(onInverterStatus);

  publish_getInverters = true;


  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


  Serial.printf("MQTT: Server:Port %s:%d\n", MqttServer, MqttPort);


  mqtt_client.setServer(MqttServer, MqttPort);
  mqtt_client.setCallback(callback);

  char    subString[20];
  sprintf(subString, "cmd/%s/#", macname);
  mqtt_client.subscribe(subString);
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void loop()
{
  const uint32_t currentMillis = millis();
  mqtt_client.loop();
  yield();
  delay(10);  // <- fixes some issues with WiFi stability

  netsgp_client->update();



  // ########################################
  // ########################################

  //----------------------------------------------------------------------------------
  if (!publish_payload && publish_esp_info) {
    payload = "{\"d\":{";
    payload += "\"Up\":";
    payload += String((unsigned long)millis() / 1000);
    // ESP
    payload += ",\"esp\":{";
#if defined(ESP8266)
    payload += "\"cpu\":\"8266\"";
#elif defined(ESP8285)
    payload += "\"cpu\":\"8285\"";
#elif defined(ESP32)
    payload += "\"cpu\":\"32\"";

#endif
#if defined(ESP8266) or defined(ESP8285)
    payload += ",\"core\":\"" + String(ESP.getCoreVersion()) + "\"";
    payload += ",\"id\":" + String(ESP.getChipId());
#endif
    payload += ",\"sdk\":\"" + String(ESP.getSdkVersion()) + "\"";
    payload += ",\"sketch\":" + String(ESP.getSketchSize());
    payload += ",\"sketch_md5\":\"" + String(ESP.getSketchMD5().c_str()) + "\"";

    payload += ",\"free\":" + String(ESP.getFreeSketchSpace());
    payload += "}";
    payload += ",\"flash\":{";
    payload += "\"sdk_size\":" + String(ESP.getFlashChipSize());
#if defined(ESP8266) or defined(ESP8285)
    payload += ",\"id\":" + String(ESP.getFlashChipId());
    payload += ",\"real_size\":" + String(ESP.getFlashChipRealSize());
#endif
    payload += "}";
    payload += ",\"wifi\":{";
    payload += "\"ap\":\"" + String(WiFi.BSSIDstr()) + "\"";
    payload += "}";
    payload += "}}";

    publish_payload = true;
    publish_esp_info = false;
  }

  if (!publish_payload && publish_ip_info) {
    payload = "{\"d\":{\"N\":";
    payload += "\"" + String(macname) + "\"";
    payload += ",\"mac\":";
    payload += "\"" + WiFi.macAddress() + "\"";
    payload += ",\"FILE\":";
    payload += "\"" + String(__FILENAME__) + "\"";
    payload += ",\"COMPILE\":";
    payload += "\"" + String(CompileDate) + "\"";
    payload += ",\"ip\":";
    payload += "\"" + WiFi.localIP().toString() + "\"";
    payload += ",\"hostname\":";
#ifdef ESP8266
    payload += "\"" + WiFi.hostname() + "\"";
#elif defined(ESP32)
    payload += "\"" + String(WiFi.getHostname()) + "\"";
#endif
    payload += ",\"Up\":";
    payload += ((unsigned long)millis() / 1000);
    payload += "}}";

    publish_payload = true;
    publish_ip_info = false;
  }

  if (!publish_payload && publish_getInverters) {
    payload = netsgp_client->getInvertersJSON();
    publish_payload = true;
    publish_getInverters = false;
  }

  if (!publish_payload && publish_Inverter) {

    Serial.print("Inverter found: ");
    uint8_t found = netsgp_client->findInverter(vInverter.deviceID);
    Serial.println(found);
    String Source = "";
    if (found > 0)Source = "own";
    if (!found)Source = "answer";

    payload = "{\"d\":{\"N\":";
    payload += "\"" + String(macname) + "\"";
    long rssi = WiFi.RSSI();
    payload += ",\"rssi\":";
    payload += String(rssi) + ",";
    payload += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    payload += ",\"Up\":";
    payload += ((unsigned long)millis() / 1000);
    payload += "}";
    char devID[20];
    sprintf(devID, "%08lX", vInverter.deviceID);
    payload += ",\"" + String(devID) + "\":{";
    payload += "\"netsgp\":\"online\"";
    payload += ",\"State\":";
    payload += String(vInverter.state);
    payload += ",\"DC_Voltage\":";
    payload += String(vInverter.dcVoltage);
    payload += ",\"DC_Current\":";
    payload += String(vInverter.dcCurrent);
    payload += ",\"AC_Voltage\":";
    payload += String(vInverter.acVoltage);
    payload += ",\"AC_Current\":";
    payload += String(vInverter.acCurrent);
    payload += ",\"AC_Gen_Total\":";
    payload += String(vInverter.totalGeneratedPower);
    payload += ",\"Temperature\":";
    payload += String(vInverter.temperature);
    payload += ",\"DC_Current\":";
    payload += String(vInverter.dcCurrent);
    payload += ",\"Source\":";
    payload += "\"" + Source + "\"" ;
    payload += "}}";
    publish_payload = true;

    publish_Inverter = false;
  }

  // ########################################
  // ########################################
  // publish payload
  if (mqtt_client.connected() && publish_payload) {

    publish_payload = false;

    Serial.printf("pl    : %s\n", payload.c_str());
    Serial.printf("pl len: %d\n", payload.length());
    if (payload.length() > 50) { //MQTT_MAX_PACKET_SIZE ) {
      Serial.printf("payload to long: %d max: %d\n", payload.length(), MQTT_MAX_PACKET_SIZE);

      mqtt_client.beginPublish(macname, payload.length(), false);
      int16_t rem = 50;
      for (uint16_t i = 0; i < payload.length(); i += rem) {
        rem = payload.length() - i;
        if (rem > 50) rem = 50;
        Serial.printf("%04d to %04d: %s\n", i, i + rem - 1, payload.substring(i, i + rem).c_str());
        mqtt_client.print(payload.substring(i, i + rem));
        yield();
      }
      mqtt_client.endPublish();
    } else {

      if (mqtt_client.publish(macname, (char*) payload.c_str())) {
        Serial.println("mqtt publish ok");
      } else {
        Serial.println("!!! mqtt publish fail !!!");
        Serial.println(payload);
      }
    }

  } // MqttClient.connected() && publish_payload

  // ########################################
  // ########################################

  if (currentMillis - lastSendMillis > 300000)
  {
    payload = "{\"d\":{";
    payload += "\"Up\":";
    payload += ((unsigned long)millis() / 1000);
    payload += "}}";
    publish_payload = true;

    lastSendMillis = currentMillis;
  }

}
