#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>    //if you get an error here you need to install the ESP8266 board manager
#include <PubSubClient.h>   //https://github.com/knolleary/pubsubclient
#include <AH_EasyDriver.h>  //http://www.alhin.de/arduino/downloads/AH_EasyDriver_20120512.zip
#include <SimpleTimer.h>    //https://github.com/marcelloromani/Arduino-SimpleTimer/tree/master/SimpleTimer
#include <ArduinoOTA.h>     //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/

#define USER_SSID             "YOUR_SSID"
#define USER_PASSWORD         "YOUR_PASSWORD"
#define USER_MQTT_SERVER      "YOUR_MQTT_IP_ADRRESS"
#define USER_MQTT_PORT        1883
#define USER_MQTT_USERNAME    "YOUR_MQTT_USERNAME"
#define USER_MQTT_PASSWORD    "YOUR_MQTT_PASSWORD#"
#define USER_MQTT_CLIENT_TYPE "Blinds"

#define HA_DEVICE_LOCATION "Office"
#define HA_DEVICE_NUMBER "1"

#define STEPPER_SPEED           35           //Defines the speed in RPM for your stepper motor
#define STEPPER_STEPS_PER_REV   1024         //Defines the number of pulses that is required for the stepper to rotate 360 degrees
#define STEPPER_MICROSTEPPING   0            //Defines microstepping 0 = no microstepping, 1 = 1/2 stepping, 2 = 1/4 stepping
#define DRIVER_INVERTED_SLEEP   1            //Defines sleep while pin high.  If your motor will not rotate freely when on boot, comment this line out.

#define STEPS_TO_CLOSE 24  //Defines the number of steps needed to open or close fully

#define STEPPER_DIR_PIN D6         
#define STEPPER_STEP_PIN D7        
#define STEPPER_SLEEP_PIN D5       
#define STEPPER_MICROSTEP_1_PIN 5  
#define STEPPER_MICROSTEP_2_PIN 4  

/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/

//Global Variables
bool boot = true;
int currentPosition = 0;
int newPosition = 0;
char positionPublish[50];
bool moving = false;
SimpleTimer timer;
bool topicExists = false; // Flag to check if the topic exists

// WiFi credentials
char ssid[32] = USER_SSID;
char password[64] = USER_PASSWORD;

// MQTT Broker
char mqtt_server[40] = USER_MQTT_SERVER;
int mqtt_port = USER_MQTT_PORT;
char mqtt_username[32] = USER_MQTT_USERNAME;
char mqtt_password[32] = USER_MQTT_PASSWORD;

// MQTT topics
char ha_device_name[64] = USER_MQTT_CLIENT_TYPE " - " HA_DEVICE_LOCATION " " HA_DEVICE_NUMBER;
char mqtt_client_id[64] = USER_MQTT_CLIENT_TYPE "_" HA_DEVICE_LOCATION "_" HA_DEVICE_NUMBER;
char mqtt_topic[64] = USER_MQTT_CLIENT_TYPE "/" HA_DEVICE_LOCATION "_" HA_DEVICE_NUMBER;

String mqtt_topic_state = String(mqtt_topic) + "/state";
String mqtt_topic_control = String(mqtt_topic) + "/control";
String mqtt_topic_status = String(mqtt_topic) + "/status";
String mqtt_topic_checkIn = String(mqtt_topic) + "/checkIn";
String mqtt_topic_availability = String(mqtt_topic) + "/availability";

const char* topic_state = mqtt_topic_state.c_str();
const char* topic_control = mqtt_topic_control.c_str();
const char* topic_status = mqtt_topic_status.c_str();
const char* topic_checkIn = mqtt_topic_checkIn.c_str();
const char* topic_availability = mqtt_topic_availability.c_str();

void convertTopicsToLowerCase() {
  int length = strlen(mqtt_topic);
  for (int i = 0; i < length; i++) {
    mqtt_topic[i] = tolower(mqtt_topic[i]);
  }

    length = strlen(mqtt_client_id);
    for (int i = 0; i < length; i++) {
        mqtt_client_id[i] = tolower(mqtt_client_id[i]);
    }

  mqtt_topic_state.toLowerCase();
  mqtt_topic_control.toLowerCase();
  mqtt_topic_status.toLowerCase();
  mqtt_topic_checkIn.toLowerCase();
  mqtt_topic_availability.toLowerCase();
}

// Create instance of AH_EasyDriver
AH_EasyDriver shadeStepper(STEPPER_STEPS_PER_REV, STEPPER_DIR_PIN, STEPPER_STEP_PIN, STEPPER_MICROSTEP_1_PIN, STEPPER_MICROSTEP_2_PIN, STEPPER_SLEEP_PIN);

// Initialize MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Function to handle MQTT messages
char charPayload[500];  // Declare charPayload variable

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {

  int retries = 0;

  // Convert mqtt_client_id to lowercase
  String mqttClientIdLower = mqtt_client_id;
  mqttClientIdLower.toLowerCase();

  // Loop until we're reconnected
  while (!client.connected()) {
    if (retries < 150) {
      Serial.print("Attempting MQTT connection...");

      if (client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
        Serial.println("connected");
        client.subscribe(topic_control);
        client.subscribe(topic_status);
        client.subscribe(topic_checkIn);
        client.subscribe(topic_availability);

        // Publish discovery message only after successful connection
        delay(1000);
        publishDiscoveryMessage();

        if (boot == false) {
          client.publish(topic_checkIn, "Reconnected");
        }
        if (boot == true) {
          client.publish(topic_checkIn, "Rebooted");
        }

        Serial.println(client.state());
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }
    if (retries > 149) {
      ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char*)payload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  // Check if the received topic is the one you're interested in
  if (String(topic) == "homeassistant/cover/" + String(mqtt_client_id) + "/config") {
    topicExists = true; // Set the flag to true if the topic exists
  }

  // Convert payload to string
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  if (newTopic == topic_control) {
    if (newPayload == "OPEN") {
      client.publish(topic_status, "0", true);
    } else if (newPayload == "CLOSE") {
      int stepsToClose = STEPS_TO_CLOSE;
      String temp_str = String(stepsToClose);
      temp_str.toCharArray(charPayload, temp_str.length() + 1);
      client.publish(topic_status, charPayload, true);
    } else if (newPayload == "STOP") {
      String temp_str = String(currentPosition);
      temp_str.toCharArray(positionPublish, temp_str.length() + 1);
      client.publish(topic_status, positionPublish, true);
    }
    Serial.print("Control change to: ");
    Serial.println(newPosition);
  } else if (String(topic) == topic_status) {
    newPosition = intPayload;  // Set target position based on the received numeric value
    Serial.print("Status change to: ");
    Serial.println(newPosition);
  }

  if (newTopic == topic_status) {
    if (boot == true) {
      newPosition = intPayload;
      currentPosition = intPayload;
      boot = false;
    }
    if (boot == false) {
      newPosition = intPayload;
    }
  }
}

void processStepper() {
  if (newPosition > currentPosition) {
// Move the stepper motor forward
#if DRIVER_INVERTED_SLEEP == 1
    shadeStepper.sleepON();
#endif
#if DRIVER_INVERTED_SLEEP == 0
    shadeStepper.sleepOFF();
#endif
    shadeStepper.move(100, FORWARD);
    currentPosition++;
    Serial.print("Closing shades  from: ");
    Serial.print(currentPosition);
    Serial.print(" to ");
    Serial.println(newPosition);
    moving = true;
  } else if (newPosition < currentPosition) {
// Move the stepper motor backward
#if DRIVER_INVERTED_SLEEP == 1
    shadeStepper.sleepON();
#endif
#if DRIVER_INVERTED_SLEEP == 0
    shadeStepper.sleepOFF();
#endif
    shadeStepper.move(100, BACKWARD);
    currentPosition--;
    Serial.print("Opening shades from: ");
    Serial.print(currentPosition);
    Serial.print(" to ");
    Serial.println(newPosition);
    moving = true;
  } else if (newPosition == currentPosition && moving == true) {
// Stop the motor and put it to sleep when it reaches the target position
#if DRIVER_INVERTED_SLEEP == 1
    shadeStepper.sleepOFF();
#endif
#if DRIVER_INVERTED_SLEEP == 0
    shadeStepper.sleepON();
#endif
    String temp_str = String(currentPosition);
    temp_str.toCharArray(positionPublish, temp_str.length() + 1);
    client.publish(topic_status, positionPublish);
    moving = false;
  }
}

void checkIn() {
  client.publish(topic_checkIn, "OK");
  client.publish(topic_availability, "online");
}

void publishDiscoveryMessage() {
  // First, subscribe to the topic to check if it exists
  client.subscribe(("homeassistant/cover/" + String(mqtt_client_id) + "/config").c_str());

  // Wait for a short period to receive a message, if any
  delay(1000);

  // Now, check if the topic exists
  if (client.connected() && !topicExists) {
    char discovery_message[500];
    snprintf(discovery_message, sizeof(discovery_message),
             "{\"name\": \"%s\", \"platform\": \"mqtt\", "
             "\"command_topic\": \"%s\", \"position_topic\": \"%s\", "
             "\"set_position_topic\": \"%s\", \"availability_topic\": \"%s\", "
             "\"payload_open\": \"OPEN\", \"payload_close\": \"CLOSE\", "
             "\"payload_stop\": \"STOP\", \"position_open\": 0, "
             "\"position_closed\": %d, \"unique_id\": \"cover.%s\"}",
             ha_device_name, topic_control, topic_status, topic_status, topic_availability,
             STEPS_TO_CLOSE, mqtt_client_id);
    client.publish(("homeassistant/cover/" + String(mqtt_client_id) + "/config").c_str(), discovery_message, true);
    Serial.println();
    Serial.print("Published discovery message: ");
    Serial.println(("homeassistant/cover/" + String(mqtt_client_id) + "/config").c_str());
    Serial.println(discovery_message);
  } else {
    Serial.println("Topic already exists or MQTT client not connected, not publishing discovery message.");
  }

  // Unsubscribe from the topic after checking
  client.unsubscribe(("homeassistant/cover/" + String(mqtt_client_id) + "/config").c_str());
}
void setup() {
  Serial.begin(115200);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  setup_wifi();

  //OTA Setup
  ArduinoOTA.setHostname(mqtt_client_id);
  ArduinoOTA.begin(); 

  // Setup MQTT client
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Convert MQTT topics to lowercase
  convertTopicsToLowerCase();

  shadeStepper.setMicrostepping(STEPPER_MICROSTEPPING);  // 0 -> Full Step
  shadeStepper.setSpeedRPM(STEPPER_SPEED);               // set speed in RPM, rotations per minute
#if DRIVER_INVERTED_SLEEP == 1
  shadeStepper.sleepOFF();
#endif
#if DRIVER_INVERTED_SLEEP == 0
  shadeStepper.sleepON();
#endif

  String mqttClientIdLower = mqtt_client_id;
  mqttClientIdLower.toLowerCase();

  timer.setInterval(((1 << STEPPER_MICROSTEPPING) * 5800) / STEPPER_SPEED, processStepper);
  timer.setInterval(90000, checkIn);

  Serial.println("Reset Reason: " + ESP.getResetReason());
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
  ESP.wdtFeed();  // Reset the watchdog timer

  // Move the stepper motor
  //processStepper();
}
