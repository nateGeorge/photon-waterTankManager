// This #include statement was automatically added by the Particle IDE.
#include "thingspeak/thingspeak.h"

#include "SparkFunMAX17043.h" // Include the SparkFun MAX17043 library

SYSTEM_MODE(SEMI_AUTOMATIC); // need to use the external antenna, so set that up first then connect to the net

double voltage = 0; // Variable to keep track of LiPo voltage
double soc = 0; // Variable to keep track of LiPo state-of-charge (SOC)
bool alert; // Variable to keep track of whether alert has been triggered

int lowWaterPin = D3;
int highWaterPin = D2;
int lowPinVal = 1;
int highPinVal = 1;
bool waterLow = false;
bool waterHigh = false;
unsigned int bounceDelay = 1100; // milliseconds to wait after a water signal has been triggered
unsigned int lastTripTime = millis();
long wakeUpTimeout = 180*60; // wake up every 3 hours even if no change in detected water height

bool waitForUpdate = false; // for updating software
unsigned int updateTimeout = 10*60*1000; // 10 min timeout for waiting for software update
unsigned int startTime = millis();
int waitLoops = 5; // loops to go through before sleeping
int loopCount = 0;
unsigned int lastTime;

bool pumpOn;

Timer sendTSdata(4000, sendTS); // bit of a delay before sending data to thingspeak to make sure all data has been recorded in the query
// and that we aren't overlapping with the TS post from the waterlevel sensor
bool TSsent = false;
bool valSet = false;


Timer lowTimer(1000, pubWaterLow);
Timer highTimer(1000, pubWaterHigh);

ThingSpeakLibrary::ThingSpeak thingspeak ("Z89VL2VH8PUTADYQ");

void setup()
{
    WiFi.selectAntenna(ANT_EXTERNAL);
    Particle.connect(); // we have to wait until we've configured to use the external antenna, otherwise it will try to use the ceramic one
    while (Particle.connected() == false) { // must be connected before we subscribe, publish, etc
        delay(500);
    }
    Particle.subscribe("jsf/waterSystem/waterTankSensor/update", eventHandler);
    Particle.subscribe("jsf/waterSystem/waterTankPump/update", eventHandler);
    Particle.publish("jsf/waterSystem/waterTankSensor/online", "true"); // subscribe to this with the API like: curl https://api.particle.io/v1/devices/events/temp?access_token=1234
    
	Serial.begin(9600); // Start serial to USB, to output debug data

	// Set up Spark variables (voltage, soc, and alert):
	Particle.variable("voltage", voltage);
	Particle.variable("soc", soc);
	Particle.variable("alert", alert);
	// To read the values from a browser, go to:
	// http://api.particle.io/v1/devices/{DEVICE_ID}/{VARIABLE}?access_token={ACCESS_TOKEN}

	// Set up the MAX17043 LiPo fuel gauge:
	lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge

	// Quick start restarts the MAX17043 in hopes of getting a more accurate
	// guess for the SOC.
	lipo.quickStart();

	// We can set an interrupt to alert when the battery SoC gets too low.
	// We can alert at anywhere between 1% - 32%:
	lipo.setThreshold(20); // Set alert threshold to 20%.
	
	voltage = lipo.getVoltage();
	// lipo.getSOC() returns the estimated state of charge (e.g. 79%)
	soc = lipo.getSOC();
	// lipo.getAlert() returns a 0 or 1 (0=alert not triggered)
	alert = lipo.getAlert();
	
	// Set up water level detection.  When water is at a certain level, the respective pin will read low (0), otherwise reads high.
	Particle.variable("waterLow", boolToText(waterLow));
	Particle.variable("waterHigh", boolToText(waterHigh));
	pinMode(lowWaterPin, INPUT_PULLUP);
	pinMode(highWaterPin, INPUT_PULLUP);
    readWaterLevel();
    lastTime = millis();
}

void loop()
{
    if (waitForUpdate || loopCount < waitLoops) {
        if (millis() - lastTime > 5000) {
        	// lipo.getVoltage() returns a voltage value (e.g. 3.93)
        	voltage = lipo.getVoltage();
        	// lipo.getSOC() returns the estimated state of charge (e.g. 79%)
        	soc = lipo.getSOC();
        	// lipo.getAlert() returns a 0 or 1 (0=alert not triggered)
        	alert = lipo.getAlert();
        
        	// Those variables will update to the Spark Cloud, but we'll also print them
        	// locally over serial for debugging:
        	Serial.print("Voltage: ");
        	Serial.print(voltage);  // Print the battery voltage
        	Serial.println(" V");
        
        	Serial.print("Alert: ");
        	Serial.println(alert);
        
        	Serial.print("Percentage: ");
        	Serial.print(soc); // Print the battery state of charge
        	Serial.println(" %");
        	Serial.println();
        	
        	if ((millis() - startTime) > updateTimeout) {
        	    waitForUpdate = false;
        	}
        	loopCount += 1;
        	lastTime = millis();
        }
    } else {
        Particle.publish("jsf/waterSystem/waterTankSensor/online", "false");
        delay(1000);
        System.sleep(SLEEP_MODE_DEEP, wakeUpTimeout);
        //goToSleep.start();
        //waitForLevelChange(); // would only work with other extra hardware, see below
    }
}

void pubWaterLow()
{
    Particle.publish("jsf/waterSystem/waterTankSensor/waterLow", boolToText(waterLow));
    lowTimer.stop();
}

void pubWaterHigh()
{
    Particle.publish("jsf/waterSystem/waterTankSensor/waterHigh", boolToText(waterHigh));
    highTimer.stop();
}

void waterLowFunc()
{
    if (millis() - lastTripTime > bounceDelay) {
        lastTripTime = millis();
        Serial.println("water low");
        waterLow = true;
        detachInterrupt(lowWaterPin);
        attachInterrupt(lowWaterPin, waterNotLowFunc, FALLING);
        lowTimer.start();
    }
}

void waterNotLowFunc()
{
    if (millis() - lastTripTime > bounceDelay) {
        lastTripTime = millis();
        Serial.println("water not low");
        waterLow = false;
        detachInterrupt(lowWaterPin);
        attachInterrupt(lowWaterPin, waterLowFunc, RISING);
        lowTimer.start();
    }
}


void waterHighFunc()
{
    if (millis() - lastTripTime > bounceDelay) {
    lastTripTime = millis();
    Serial.println("water high");
    waterHigh = true;
    detachInterrupt(highWaterPin);
    attachInterrupt(highWaterPin, waterNotHighFunc, RISING);
    highTimer.start();
    }
}

void waterNotHighFunc()
{
    if (millis() - lastTripTime > bounceDelay) {
    lastTripTime = millis();
    Serial.println("water not high");
    waterHigh = false;
    detachInterrupt(highWaterPin);
    attachInterrupt(highWaterPin, waterHighFunc, FALLING);
    highTimer.start();
    }
}

void eventHandler(String event, String data)
{
  // to publish update: curl https://api.particle.io/v1/devices/events -d "name=update" -d "data=true" -d "private=true" -d "ttl=60" -d access_token=1234
  if (event == "jsf/waterSystem/waterTankSensor/update") {
      (data == "true") ? waitForUpdate = true : waitForUpdate = false;
      if (waitForUpdate) {
        Serial.println("wating for update");
        Particle.publish("jsf/waterSystem/waterTankSensor/updateConfirm", "waiting for update");
      } else {
        Serial.println("not wating for update");
        Particle.publish("jsf/waterSystem/waterTankSensor/updateConfirm", "not waiting for update");
      }
  } else if (event == "jsf/waterSystem/waterTankPump/pumpOn") {
      (data == "true") ? pumpOn = true : pumpOn = false;
      Serial.println("pumpOn:");
      Serial.println(String(pumpOn));
  }
  Serial.print(event);
  Serial.print(", data: ");
  Serial.println(data);
}

void readWaterLevel() {
    lastTripTime = millis();
    detachInterrupt(lowWaterPin);
    detachInterrupt(highWaterPin);
    lowPinVal = digitalRead(lowWaterPin);
	highPinVal = digitalRead(highWaterPin);
	if (lowPinVal == 1) {
	    waterLow = true;
	    Particle.publish("jsf/waterSystem/waterTankSensor/waterLow", "true");
	    //attachInterrupt(lowWaterPin, waterNotLowFunc, FALLING);
	} else {
	    waterLow = false; // water is covering the waterLow sensor
	    Particle.publish("jsf/waterSystem/waterTankSensor/waterLow", "false");
	    //attachInterrupt(lowWaterPin, waterLowFunc, RISING);
	}
	if (highPinVal == 1) {
	    waterHigh = false;
	    Particle.publish("jsf/waterSystem/waterTankSensor/waterHigh", "false");
	    //attachInterrupt(highWaterPin, waterHighFunc, FALLING);
	} else {
	    waterHigh = true; // water is covering the waterLow sensor
	    Particle.publish("jsf/waterSystem/waterTankSensor/waterHigh", "true");
	    //attachInterrupt(highWaterPin, waterNotHighFunc, RISING);
	}
	valSet = thingspeak.recordValue(2, String(boolToNum(waterLow)));
	valSet = thingspeak.recordValue(3, String(boolToNum(waterHigh)));
    valSet = thingspeak.recordValue(5, String(soc));
	valSet = thingspeak.recordValue(6, String(voltage));
    sendTSdata.start();
}

int boolToNum(bool thing)
{
    int result;
    thing ? result = 1 : result = 0;
    return result;
}

String boolToText(bool thing)
{
    String result;
    thing ? result = "true" : result = "false";
    return result;
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

void waitForLevelChange()
{
    // unfortunately, this would only work with some extra hardware, would need a pullup/gnd connection to the battery or solar cell
    /*
    
    if (waterLow) {
        // wait for water to touch waterLowPin
        System.sleep(lowWaterPin, FALLING, wakeUpTimeout);
    } else {
        if (waterHigh) {
            // wait for water to stop touching waterHighPin
            System.sleep(highWaterPin, RISING, wakeUpTimeout);
        } else { // water level in between high and low sensors
            if (pumpOn) {
                // tank filling, wait for water to hit waterHighPin
                System.sleep(highWaterPin, FALLING, wakeUpTimeout);
            } else {
                // tank emptying, wait for water to stop touching waterLowPin
                System.sleep(lowWaterPin, RISING, wakeUpTimeout);
            }
        }
    }
    */
}