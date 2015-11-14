// This #include statement was automatically added by the Particle IDE.
#include "thingspeak/thingspeak.h"

bool pumpOn = false;
bool autoMode = false;
bool waterLow = false;
bool waterHigh = false;
int success;
int relayPin = 5;
unsigned int relayStartTime;
unsigned int lastSignal = millis();
unsigned int pumpTimeout = 15*60*1000; // turn off the pump if haven't heard from sensor in this time
unsigned int pumpOnTime = 15*60*1000; // pump run time in manual mode
ThingSpeakLibrary::ThingSpeak thingspeak ("Z89VL2VH8PUTADYQ");

Timer sendTSdata(2500, sendTS); // bit of a delay before sending data to thingspeak to make sure all data has been recorded in the query
// and that we aren't overlapping with the TS post from the waterlevel sensor
bool TSsent = false;
bool valSet = false;

void setup() {
    Particle.publish("jsf/waterSystem/waterTankPump/online", "true", 90);
    Particle.function("relayOn", relayOn);
    Particle.function("relayOff", relayOff);
    
    Particle.variable("pumpOn", boolToText(pumpOn));
    
    Particle.subscribe("jsf/waterSystem/", eventHandler);
    
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);
}

void loop() {
    if (pumpOn) {
        if (autoMode) {
            if (millis() - relayStartTime > pumpTimeout) {
                relayOff("bullshit");
            }
        } else {
            if (millis() - relayStartTime > pumpOnTime) {
                relayOff("bullshit");
            }
        }
    }
}

int relayOn(String bullshit)
{
	digitalWrite(relayPin, HIGH);
	pumpOn = true;
	Particle.publish("jsf/waterSystem/waterTankPump/pumpOn", boolToText(pumpOn));
	valSet = thingspeak.recordValue(1, String(boolToNum(pumpOn)));
	sendTSdata.start();
	relayStartTime = millis();
	return 1;
}

int relayOff(String bullshit)
{
	digitalWrite(relayPin, LOW);
	pumpOn = false;
	Particle.publish("jsf/waterSystem/waterTankPump/pumpOn", boolToText(pumpOn));
	valSet = thingspeak.recordValue(1, String(boolToNum(pumpOn)));
	sendTSdata.start();
	return 1;
}

String boolToText(bool thing)
{
    String result;
    thing ? result = "true" : result = "false";
    return result;
}

String boolToOnOff(bool thing)
{
    String result;
    thing ? result = "On" : result = "Off";
    return result;
}

int boolToNum(bool thing)
{
    int result;
    thing ? result = 1 : result = 0;
    return result;
}

void eventHandler(String event, String data)
{
  if (event == "jsf/waterSystem/waterTankSensor/online") {
      Particle.publish("jsf/waterSystem/waterTankPump/pumpOn", boolToText(pumpOn));
      valSet = thingspeak.recordValue(4, String(boolToNum(autoMode)));
      valSet = thingspeak.recordValue(1, String(boolToNum(pumpOn)));
	  sendTSdata.start();
  } else if (event == "jsf/waterSystem/waterTankSensor/waterLow") {
      (data == "true") ? waterLow = true : waterLow = false;
      if (autoMode == true) {autoPumpControl();}
      lastSignal = millis();
  } else if (event == "jsf/waterSystem/waterTankSensor/waterHigh") {
      (data == "true") ? waterHigh = true : waterHigh = false;
      if (autoMode == true) {autoPumpControl();}
      lastSignal = millis();
  } else if (event == "jsf/waterSystem/waterTankPump/autoMode") {
      (data == "true") ? autoMode = true : autoMode = false;
      if (autoMode == true) {
          autoPumpControl();
          Particle.publish("jsf/waterSystem/waterTankPump/autoModeConfirm", "true");
      } else {
          Particle.publish("jsf/waterSystem/waterTankPump/autoModeConfirm", "false");
          relayOff("bullshit");
      }
      valSet = thingspeak.recordValue(4, String(boolToNum(autoMode)));
  }
  Serial.print(event);
  Serial.print(", data: ");
  Serial.println(data);
}

void sendTS()
{
    if (TSsent == true || valSet == false) {
        sendTSdata.stop();
        TSsent = false;
        valSet = false;
    } else {
        TSsent = thingspeak.sendValues();
    }
}

void autoPumpControl() {
    if (waterLow == true) {
        success = relayOn("bullshit");
    } else if (waterHigh == true) {
        success = relayOff("bullshit");
    }
}