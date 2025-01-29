#include <Arduino.h>
#include <AzIoTSasToken.h>
#include <SerialLogger.h>
#include <WiFi.h>
#include <az_core.h>
#include <azure_ca.h>
#include <ctime>
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"

/* Azure auth data */
char* deviceKey = "RAmJI0YX7duvqDP7vChVv6a2VaPoMuoflAIoTB7kQwM=";	 // Azure Primary key for device
const char* iotHubHost = "foi-rus-lposta21.azure-devices.net";		 //[Azure IoT host name].azure-devices.net
const char* deviceId = "ESP32_WROVER";  // Device ID as specified in the list of devices on IoT Hub
const int tokenDuration = 60;

/* MQTT data for IoT Hub connection */
const char* mqttBroker = iotHubHost;  // MQTT host = IoT Hub link
const int mqttPort = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;	// Secure MQTT port
const char* mqttC2DTopic = AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC;	// Topic where we can receive cloud to device messages

// These three are just buffers - actual clientID/username/password is generated
// using the SDK functions in initIoTHub()
char mqttClientId[128];
char mqttUsername[128];
char mqttPasswordBuffer[200];
char publishTopic[200];

/* Auth token requirements */

uint8_t sasSignatureBuffer[256];  // Make sure it's of correct size, it will just freeze otherwise :/

az_iot_hub_client client;
AzIoTSasToken sasToken(
	&client, az_span_create_from_str(deviceKey),
	AZ_SPAN_FROM_BUFFER(sasSignatureBuffer),
	AZ_SPAN_FROM_BUFFER(
		mqttPasswordBuffer));	 // Authentication token for our specific device

/* Pin definitions and library instance(s) */

#define PIR_PIN 14    // GPIO za PIR senzor
#define RED_PIN 18    // GPIO za crvenu LED
#define GREEN_PIN 19  // GPIO za zelenu LED



/* WiFi things */

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

const char* ssid = "Desmond Benjamin";
const char* pass = "forL14pass";


void setupWiFi() {
	Logger.Info("Connecting to WiFi");

	wifiClient.setCACert((const char*)ca_pem); // We are using TLS to secure the connection, therefore we need to supply a certificate (in the SDK)

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

    short timeoutCounter = 0;
	while (WiFi.status() != WL_CONNECTED) { // Wait until we connect...
		Serial.print(".");
		delay(500);

		timeoutCounter++;
		if (timeoutCounter >= 20) ESP.restart(); // Or restart if we waited for too long, not much else can you do
	}

	Logger.Info("WiFi connected");
}




// Use pool pool.ntp.org to get the current time
// Define a date on 1.1.2023. and wait until the current time has the same year (by default it's 1.1.1970.)
void initializeTime() {	 // MANDATORY or SAS tokens won't generate
  Logger.Info("Setting time using SNTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(NULL);
  std::tm tm{};
  tm.tm_year = 2023; // Define a date on 1.1.2023. and wait until the current time has the same year (by default it's 1.1.1970.)

  while (now < std::mktime(&tm)) // Since we are using an Internet clock, it may take a moment for clocks to sychronize
  {
    delay(500);
    Serial.print(".");
    now = time(NULL);
  }
}



void setupPIRSensor() {
    pinMode(PIR_PIN, INPUT);
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);

    // Početno stanje LED
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
}


// MQTT is a publish-subscribe based, therefore a callback function is called whenever something is published on a topic that device is subscribed to
void callback(char *topic, byte *payload, unsigned int length) { 
  payload[length] = '\0'; // It's also a binary-safe protocol, therefore instead of transfering text, bytes are transfered and they aren't null terminated - so we need ot add \0 to terminate the string
  String message = String((char *)payload); // After it's been terminated, it can be converted to String

  Logger.Info("Callback:" + String(topic) + ": " + message);
}

void connectMQTT() {
    mqttClient.setBufferSize(1024);
    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setCallback(callback);

    while (!mqttClient.connected()) {
        Logger.Info("Attempting MQTT connection...");
        if (sasToken.Generate(tokenDuration) != 0) {
            Logger.Error("Failed generating SAS token");
            return;
        }

        const char* mqttPassword = (const char*)az_span_ptr(sasToken.Get());
        if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
            Logger.Info("MQTT connected");
            mqttClient.subscribe(mqttC2DTopic);
        } else {
            Logger.Info("Trying again in 5 seconds");
            delay(5000);
        }
    }
}



String getTelemetryData() {
    StaticJsonDocument<128> doc;
    String output = "";

    doc["DeviceID"] = (String)deviceId;

    serializeJson(doc, output);
    Logger.Info(output);
    return output;
}


void sendTelemetryData() {
  String telemetryData = getTelemetryData();
  mqttClient.publish(publishTopic, telemetryData.c_str());
}

long lastTime, currentTime = 0;
unsigned long lastMotionTime = 0; 
const unsigned long noMotionDelay = 10000;    // Vrijeme neaktivnosti prije povratka u "slobodno" stanje
const unsigned long debounceDelay = 1000;    // Minimalni razmak između detekcija (debouncing)
unsigned long lastDebounceTime = 0;          // Vrijeme zadnje registracije pokreta
unsigned long lastCheckTime = 0;             // Vrijeme zadnje provjere u loop() petlji
const unsigned long checkInterval = 100;     // Interval između provjera
int motionCount = 0;                         // Brojač pokreta
bool firstMessage = true;                    // Indikator za prvu poruku

int interval = 5000;
void checkTelemetry() { // Do not block using delay(), instead check if enough time has passed between two calls using millis() 
  currentTime = millis();

  if (currentTime - lastTime >= interval) { // Subtract the current elapsed time (since we started the device) from the last time we sent the telemetry, if the result is greater than the interval, send the data again
    Logger.Info("Sending telemetry...");
    sendTelemetryData();

    lastTime = currentTime;
  }
}

unsigned long lastInactiveTimeReported = 0; // Dodana varijabla za praćenje posljednje prijavljene sekunde

unsigned long startTime = millis(); // Dodana varijabla za praćenje početnog vremena

void checkPIRSensor() {
  unsigned long currentTime = millis();

  // Provjera svakih 100 ms
  if (currentTime - lastCheckTime >= checkInterval) {
    lastCheckTime = currentTime;

    int motionDetected = digitalRead(PIR_PIN);

    // Ako je detektiran pokret
    if (motionDetected == HIGH && (currentTime - lastDebounceTime > debounceDelay)) {
      lastDebounceTime = currentTime; // Postavi vrijeme zadnje detekcije
      firstMessage = false; // Nakon prvog pokreta, isključuje se početna poruka

      digitalWrite(RED_PIN, HIGH);
      digitalWrite(GREEN_PIN, LOW);
      lastMotionTime = currentTime; // Ažuriraj vrijeme zadnjeg pokreta
      lastInactiveTimeReported = 0; // Resetiraj prijavljeni broj sekundi
      Logger.Info("Pokret detektiran! Resetiranje brojača neaktivnosti.");

      // Pošalji podatke o pokretu na IoT Hub
      sendTelemetryData();
    } 
    // Ako još nije bilo pokreta (brojanje od 0)
    else if (firstMessage) {
      unsigned long inactiveTime = (currentTime - startTime) / 1000; // Vrijeme od pokretanja uređaja u sekundama
      if (inactiveTime != lastInactiveTimeReported) {
        Logger.Info("Sekunde prije prvog pokreta: " + String(inactiveTime));
        lastInactiveTimeReported = inactiveTime; // Ažuriraj zadnji prijavljeni broj sekundi
      }
    }
    // Ako nije bilo pokreta više od 10 sekundi
    else if (currentTime - lastMotionTime >= noMotionDelay) {
      unsigned long inactiveTime = ((currentTime - lastMotionTime - noMotionDelay) / 1000) + 10; // Početak brojanja od 10
      if (inactiveTime != lastInactiveTimeReported) {
        Logger.Info("Sekunde bez pokreta: " + String(inactiveTime));
        lastInactiveTimeReported = inactiveTime; // Ažuriraj zadnji prijavljeni broj sekundi
      }

      digitalWrite(RED_PIN, LOW);
      digitalWrite(GREEN_PIN, HIGH);
    }
  }
}



void sendTestMessageToIoTHub() {
  az_result res = az_iot_hub_client_telemetry_get_publish_topic(&client, NULL, publishTopic, 200, NULL ); // The receive topic isn't hardcoded and depends on chosen properties, therefore we need to use az_iot_hub_client_telemetry_get_publish_topic()
  Logger.Info(String(publishTopic));
  
  mqttClient.publish(publishTopic, deviceId); // Use https://github.com/Azure/azure-iot-explorer/releases to read the telemetry
}

bool initIoTHub() {
  az_iot_hub_client_options options = az_iot_hub_client_options_default(); // Get a default instance of IoT Hub client options

  if (az_result_failed(az_iot_hub_client_init( // Create an instnace of IoT Hub client for our IoT Hub's host and the current device
          &client,
          az_span_create((unsigned char *)iotHubHost, strlen(iotHubHost)),
          az_span_create((unsigned char *)deviceId, strlen(deviceId)),
          &options)))
  {
    Logger.Error("Failed initializing Azure IoT Hub client");
    return false;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqttClientId, sizeof(mqttClientId) - 1, &client_id_length))) // Get the actual client ID (not our internal ID) for the device
  {
    Logger.Error("Failed getting client id");
    return false;
  }

  size_t mqttUsernameSize;
  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqttUsername, sizeof(mqttUsername), &mqttUsernameSize))) // Get the MQTT username for our device
  {
    Logger.Error("Failed to get MQTT username ");
    return false;
  }

  Logger.Info("Great success");
  Logger.Info("Client ID: " + String(mqttClientId));
  Logger.Info("Username: " + String(mqttUsername));

  return true;
}

void setup() {
  setupWiFi();
  initializeTime();

  if (initIoTHub()) {
    connectMQTT();
  }

  sendTestMessageToIoTHub();

    setupPIRSensor();

  Logger.Info("Setup done");
}


void loop() {
    if (!mqttClient.connected()) connectMQTT();
    mqttClient.loop();
    checkTelemetry();
    checkPIRSensor();
}

