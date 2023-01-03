#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include <Ticker.h>
#include <ESP32SharpIR.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Ubidots.h"
#include <string>

using namespace std;

#ifndef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP32 ONLY!)
#error Select ESP32 board.
#endif

/**************************************************************/
/* Example how to read DHT sensors from an ESP32 using multi- */
/* tasking.                                                   */
/* This example depends on the Ticker library to wake up      */
/* the task every 20 seconds                                  */
/**************************************************************/


const char* UBIDOTS_TOKEN = "BBFF-o6XQORupV7uiOGewqZCAFRcK3b3GRa";                                     
const char* WIFI_SSID = "carlo";                                         // Put here your Wi-Fi SSID
const char* WIFI_PASS = "12345678";                                         // Put here your Wi-Fi password
const char* DEVICE_LABEL_TO_RETRIEVE_VALUES_FROM = "demo";  // Replace with your device label
const char* VARIABLE_LABEL_TO_RETRIEVE_VALUES_FROM = "1";       // Replace with your variable label 

Ubidots ubidots(UBIDOTS_TOKEN, UBI_HTTP);

#define REDLIGHT 14

// OLED screen
#define OLED_SDA 4
#define OLED_SCL 15 
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// left motor
#define motorl_1 2
#define motorl_2 18
#define motorl_en 5

// right motor
#define motorr_1 21
#define motorr_2 13
#define motorr_en 12

// directions
#define FORWARD 1
#define REVERSE 2
#define RIGHT 3
#define LEFT 4

// buzzer 
#define buzzer 27

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

DHTesp dht;
ESP32SharpIR sensor( ESP32SharpIR::GP2Y0A21YK0F, 35);

void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();

void forward();
void reverse();
void left();
void right();
void stop4w();

int turnTime = 1000; // turn duration
int forwardTime = 2000; // forward duration

boolean turning = false; // check for turning

String direction;

/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Ticker for temperature reading */
Ticker tempTicker;
/** Comfort profile */
ComfortState cf;
/** Flag if task should run */
bool tasksEnabled = false;
/** Pin number for DHT11 data pin */
int dhtPin = 17;
unsigned long dhtInitialTime = 0; // for timer on dht sensor
float temperature;
float humidity;

int pathIndex = 0; // path index on cloud
unsigned long initialPathTime = 0; // for timer on path timer
float distanceS = 0; // variable distance

boolean redOn = false; // red light

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Initializing OLED.");
  initTemp();
  tasksEnabled = false;
  // Signal end of setup() to tasks
  sensor.setFilterRate(0.1f);
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // ubidots init
  ubidots.wifiConnect(WIFI_SSID, WIFI_PASS);

  // set OLED
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // setup buzzer and red
  pinMode(buzzer, OUTPUT);
  pinMode(REDLIGHT, OUTPUT);

  // setup for en motor
  pinMode(motorl_en, OUTPUT);
  pinMode(motorr_en, OUTPUT);
  digitalWrite(motorl_en, HIGH);
  digitalWrite(motorr_en, HIGH);

  // setup for 4 motors
  pinMode(motorl_1, OUTPUT);
  pinMode(motorl_2, OUTPUT);
  pinMode(motorr_1, OUTPUT);
  pinMode(motorr_2, OUTPUT);

  // print initialization
  printInitialize();
  clearScreen();
  print1stLine("Initialized!");

  // getTemp() for initialization of DHT
  getTemperature();
  delay(2000);
  Serial.println("End initialization");
}

void loop() {
  Serial.println("-----------------------");
  // get path and check
  int path = static_cast<int>(getPath());
//  int path = 1;
  switch (path) {
    case 1:
      forward(); 
      break;
    case 2:
      reverse();
      break;
    case 3:
      right();
      break;
    case 4:
      left();
      break;
    default:
      direction = "ERROR";
  }

  // loop duration depends on when the car is turning or is moving forward
  initialPathTime = millis();
  int duration = path == 1 ? forwardTime : turnTime;
  printLoop(initialPathTime, duration);
  if (turning) {
    turning = false;
  }
  stop4w();
  printLoop(millis(), 500);  
  updateTemperature();
}

// update temperature on cloud
void updateTemperature() {
  ubidots.add("10", temperature);
  while(!ubidots.send(DEVICE_LABEL_TO_RETRIEVE_VALUES_FROM)) {
    Serial.println("Error sending to ubidots");
  }
}

// print loop for printing temp and dist and also check obstacle and buzzer
void printLoop(int initial, int dur) {
  while (millis() - initial < dur) {
    clearScreen();
    printTempNDist(direction);
    checkObstacle();
//    checkBuzzer();  
    delay(100);
  }
}

// move forward
void forward() {
  Serial.println("GOING FORWARD");
  direction = "GOING FORWARD";
  digitalWrite(motorl_1 , HIGH);
  digitalWrite(motorl_2 , LOW);

  digitalWrite(motorr_1, HIGH);
  digitalWrite(motorr_2, LOW);
}

// stop 
void stop4w() {
  digitalWrite(motorl_1, LOW);
  digitalWrite(motorl_2, LOW);

  digitalWrite(motorr_1, LOW);
  digitalWrite(motorr_2, LOW);
}

// go reverse
void reverse() {
      direction = "REVERSING";
  Serial.println("REVERSING");
  digitalWrite(motorl_1, LOW);
  digitalWrite(motorl_2, HIGH);

  digitalWrite(motorr_1, LOW);
  digitalWrite(motorr_2, HIGH);
}

// turning left
void left() {
  turning = true;
      direction = "TURNING LEFT";
  digitalWrite(motorl_1, LOW);
  digitalWrite(motorl_2, HIGH);

  digitalWrite(motorr_1, HIGH);
  digitalWrite(motorr_2, LOW);
}

// turning right
void right() {
  turning = true;
      direction = "TURNING RIGHT";
  digitalWrite(motorl_1, HIGH);
  digitalWrite(motorl_2, LOW);

  digitalWrite(motorr_1, LOW);
  digitalWrite(motorr_2, HIGH);
}

// print temp and distance
void printTempNDist(String s) {
    if (millis() - dhtInitialTime > 3000 ) {
      getTemperature();
    }
    distanceS = sensor.getDistanceFloat();
    Serial.println(distanceS);
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Distance: ");
    display.setCursor(75, 0);
    display.print(distanceS);
    display.setCursor(0, 25);
    display.print("Temperature:");
    display.setCursor(75, 25);
    display.print(temperature);
    display.setCursor(0, 50);
    display.print(s);
    display.display();
}

// check obstacle and turn if theres obstacle
void checkObstacle() {
  if (distanceS <= 15 && redOn == false && turning == false) {
    redOn = true;
    digitalWrite(REDLIGHT, HIGH);
    Serial.println("Obstacle");  
    clearScreen();
    print1stLine("OBSTACLE DETECTED");
    tone(buzzer, 50, 1000);
    stop4w();
    delay(1000);
    right();
    unsigned long initial = millis();
    printLoop(initial, turnTime * 2);
    turning = false;
    redOn = false;
    stop4w();
    resetPath();
    printLoop(millis(), 1000);
    digitalWrite(REDLIGHT, LOW);
    noTone(buzzer);
  }
  if (distanceS > 15 && redOn == true) {
    redOn = false;
    digitalWrite(REDLIGHT, LOW);
    Serial.println("No obstacle");
  }
}

// print initialization text
void printInitialize() {
  display.setCursor(0,0);
  int i = 0;
  for (i = 0; i < 3; i++) {
    display.print("Initializing.");
    display.display();
    delay(250);
    clearScreen();
    display.setCursor(0,0);
    display.print("Initializing..");
    display.display();
    delay(250);
    clearScreen();
    display.setCursor(0,0);
    display.print("Initializing...");
    display.display();
    delay(250);
    clearScreen();
  }
}

// print 1st line on OLED
void print1stLine(float s) {
  display.setCursor(0,0);
  display.print(s);
  display.display();
}

// print 1st line on OLED
void print1stLine(String s) {
  display.setCursor(0,0);
  display.print(s);
  display.display();
}

// clear display on screen
void clearScreen() {
  display.clearDisplay();
}

/**
 * initTemp
 * Setup DHT library
 * Setup task and timer for repeated measurement
 * @return bool
 *    true if task and timer are started
 *    false if task or timer couldn't be started
 */
bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT initiated");

  // Start task to get temperature
//  xTaskCreatePinnedToCore(
//      tempTask,                       /* Function to implement the task */
//      "tempTask ",                    /* Name of the task */
//      4000,                           /* Stack size in words */
//      NULL,                           /* Task input parameter */
//      5,                              /* Priority of the task */
//      &tempTaskHandle,                /* Task handle. */
//      1);                             /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 20 seconds
    tempTicker.attach(20, triggerGetTemp);
  }
  return true;
}

/**
 * triggerGetTemp
 * Sets flag dhtUpdated to true for handling in loop()
 * called by Ticker getTempTimer
 */
void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
     xTaskResumeFromISR(tempTaskHandle);
  }
}

// reset pathIndex
void resetPath() {
  pathIndex = 0;
}

/**
 * Task to reads temperature from DHT11 sensor
 * @param pvParameters
 *    pointer to task parameters
 */
void tempTask(void *pvParameters) {
  Serial.println("tempTask loop started");
  while (1) // tempTask loop
  {
    if (tasksEnabled) {
      // Get temperature values
      getTemperature();
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

/**
 * getTemperature
 * Reads temperature from DHT11 sensor
 * @return bool
 *    true if temperature could be aquired
 *    false if aquisition failed
*/
bool getTemperature() {
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  String comfortStatus;
  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Comfort_OK";
      break;
    case Comfort_TooHot:
      comfortStatus = "Comfort_TooHot";
      break;
    case Comfort_TooCold:
      comfortStatus = "Comfort_TooCold";
      break;
    case Comfort_TooDry:
      comfortStatus = "Comfort_TooDry";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Comfort_TooHumid";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Comfort_HotAndHumid";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Comfort_HotAndDry";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Comfort_ColdAndHumid";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Comfort_ColdAndDry";
      break;
    default:
      comfortStatus = "Unknown:";
      break;
  };

  dhtInitialTime = millis();
  temperature = newValues.temperature;
  humidity = newValues.humidity;
  Serial.println(" T:" + String(newValues.temperature) + " H:" + String(newValues.humidity) + " I:" + String(heatIndex) + " D:" + String(dewPoint) + " " + comfortStatus);
  return true;
}

// get path from cloud
float getPath() {
  Serial.print("pathIndex ");
  Serial.println(pathIndex + 1);
  string pathIndexStr = to_string(pathIndex + 1);
  const char* temp = pathIndexStr.c_str();
  pathIndex = (pathIndex + 1) % 9;
  float value = ERROR_VALUE;
  while(value == ERROR_VALUE) {
    value = ubidots.get(DEVICE_LABEL_TO_RETRIEVE_VALUES_FROM, temp);
    Serial.print("Error ubidots get. ");
    Serial.println(value);
  }
  Serial.print("value ");
  Serial.println(value);
  return value;
  
}
