//-----Libs-----
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

//---Timer last update----
unsigned long timeAlarm = 0;
unsigned long timePIR = 0;
unsigned long isHomeTime = 0;
unsigned long postToThingspeak_time = 0;
unsigned long varTime = 0;
unsigned long rolloverTime = 0;
unsigned long previousMillis = 0;
unsigned long timerSMS = 0;

//----Functions
void blinker();
void pirCounter();
void listenForAlarm();
void testAlarm();
void stopAlarm();
void isHome();

//------Wifi-----
char ssid[] = "xxxxxxxxx";  //  your network SSID (name)
char pass[] = "xxxxxxxxxxxx";       // your network password
WiFiClient client;
long rssi = WiFi.RSSI();

//----LEDs----
int blueLedPin = 2;  // Blue Led
int greanLedPin1 = 15; // Green Led D8
int greanLedPin2 = 13; // Green Led D7
boolean ledState;

//---PIR-----
int pirPin = 12; // Input for HC-S501
int pirValue; // Place to store read PIR Value
int pirCount = 0;
int isHomeCount = 0;
String message;


//-----ThingSpeak-----
// replace with your channel’s thingspeak API key
String writeApiKey = "xxxxxxxxxxxxxxx";
String readApiKey = "xxxxxxxxxxxxxxx";
const char* server = "api.thingspeak.com";

//-------Blynk-----
char auth[] = "xxxxxxxxxxxxxxxxxxxxxxxxx";
String txt;
WidgetRTC rtc;
BLYNK_ATTACH_WIDGET(rtc, V5);
String currentTime;

//-------SmokeDetectorButton-----
int sdButton = 5;
int soundAlarmCount = 0; //Counts the number of time Analog signal is received before stopping the alarm
int alarmValue; //Analog value
String alarmMessage;


//------IFTTT
const uint16_t port = 80;

//-------END---------

//------FUNCTIONS--------

void blinker() {  //Blink a led
  digitalWrite(greanLedPin2, HIGH);
  delay(50);
  digitalWrite(greanLedPin2, LOW);
}

void requestIFTTT() {  //Send SMS notification in case alarm goes off via IFTTT service
  const char * host = "maker.ifttt.com";
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  String url = "/trigger/xxxxxxxxxx/with/key/yyyyyyyyyyyyyyyy"; //Where XXXX=event_name, YYYY=user's key
  Serial.print("Requesting URL: ");
  Serial.println(url);
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection");
}

void pirCounter() {  //Movement counter
  pirValue = digitalRead(pirPin);
  digitalWrite(greanLedPin1, pirValue);
  if (pirValue) {
    pirCount++;
    isHomeCount++;
    Serial.println("Motion Detected");
  }
}
void testAlarm() {  //Alarm test function
  digitalWrite(sdButton, LOW);
  Serial.println("Button pressed!!!!!!!!!!");
  delay(2000);
  digitalWrite(sdButton, HIGH);
  Serial.println("Test Alarm");
}

void listenForAlarm() {  //Listens for the smoke detector's buzzer to go off
  alarmValue = analogRead(A0);
  if ( alarmValue > 500) {
    soundAlarmCount++;
    Serial.println("ALARM!!!");
    digitalWrite(blueLedPin, LOW);
    digitalWrite(greanLedPin1, HIGH);
    digitalWrite(greanLedPin2, HIGH);
    Blynk.notify("!!!ALARM!!!");
    if (millis() - timerSMS > 60000) { //Send SMS if for the last 1 minute it hasn't been already sent
      requestIFTTT();
      timerSMS = millis();
      Serial.println("SMS Notification Sent");
    }
  } else {
    digitalWrite(blueLedPin, HIGH);
  }
}

void stopAlarm() {  //Function for stopping the alarm
  digitalWrite(sdButton, LOW);
  delay(1000);
  digitalWrite(sdButton, HIGH);
  soundAlarmCount = 0;
  //When alarm is stopped, counter is reset
  Serial.println("Stop Alarm");
}

void isHome() { //is anybody home. It will be executed every 30 seconds. If somebody is at home, message is positive, if somebody was home but isn't anymore, positive value will still be saved for 10 minutes(600000ms) after the last positive event.
  if (isHomeCount > 0) {
    message = "Somebody is at Home";
  } else {
    if (millis() - varTime > 600000) {
      message = "Nobody is at Home";
      varTime = millis();
    }
  }
}

void timeRollover() { //unsigned long value can hold millisecond count for ~50 days so some time before the rollover values are reset.
  if (millis() > 4000000000) {
    timeAlarm = 0;
    timePIR = 0;
    isHomeTime = 0;
    postToThingspeak_time = 0;
    varTime = 0;
    rolloverTime = 0;
  }
}

// field variable should be something like this
// field =     postStr +="&field1=";    postStr += String(variable);
// where "field1" is the name of the field in thingspeak and variable is the value of the field
// The "field" value can consist of multiple field[x] definitions

void thingspeak(String field) {
  if (client.connect(server, 80)) { // "184.106.153.149" or api.thingspeak.com
    String postStr = writeApiKey;
    postStr += field;
    postStr += "\r\n\r\n";
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + writeApiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    Serial.println("Sent to Thingspeak");
  }
  client.stop();
}

void postToThingspeak() {
  String postLine = "&field1=";
  postLine += String(WiFi.RSSI());
  postLine += "&field2=";
  postLine += String(pirCount / 3); //divided by 3 because the PIR will generate 3-6 counts for each detection
  postLine += "&field3=";
  postLine += String(soundAlarmCount);
  postLine += "&field4=";
  postLine += String(millis() / 8640000); //Days online. Will be restet about every 50 days
  thingspeak(postLine);
  blinker();
  pirCount = 0; //After each post to Thingspeak, counter is reset
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  // print the received signal strength:
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

//----Blynk functions-----

BLYNK_WRITE(V0)
{
  Serial.println("Message: Test Alarm"); //Button 1 to test the alarm
  testAlarm();
}
BLYNK_WRITE(V1)
{
  Serial.println("Message: Stop Alarm"); //Button 2 to stop the alarm
  stopAlarm();
}

BLYNK_READ(V2) { //Post info to Blynk

  currentTime = "-    ";
  currentTime += String(hour()) + ":" + minute() + ":" + second() + "    -";
  txt = "Movement activity: ";
  txt += String((pirCount / 3)); //divided by 3 because the PIR will generate 3-6 counts for each detection
  Blynk.virtualWrite(V2, currentTime);   // Send time to the App
  Blynk.virtualWrite(V3, txt); //Movement activity message
  Blynk.virtualWrite(V4, message); // Message about activity in the last 10 minutes
  blinker(); //Blink a led on post
}


//-------SETUP--------

void setup() {

  Serial.begin(115200);

  //---------WIFI--------
  // Connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  printWifiStatus();

  //-----Blynk-----
  Blynk.config(auth);

  while (Blynk.connect() == false) {
    // Wait until connected
  }
  // Begin synchronizing time
  rtc.begin();

  //-------LED-------
  pinMode(blueLedPin, OUTPUT);
  pinMode(greanLedPin1, OUTPUT);
  pinMode(greanLedPin2, OUTPUT);

  //-----PIR Sensor----
  pinMode(pirPin, INPUT);
  digitalWrite(blueLedPin, HIGH);

  //-----SmokeDetector-----
  pinMode(A0, INPUT);
  pinMode(sdButton, OUTPUT);
  digitalWrite(sdButton, HIGH);
}

void loop() {
  Blynk.run(); //Runs all Blynk activities

  if (millis() - timeAlarm > 100) {
    listenForAlarm();
    timeAlarm = millis();
    Serial.println("Listen for Alarm");
  }

  if (millis() - timePIR > 500) {
    pirCounter();
    timePIR = millis();
  }

  if (millis() - isHomeTime > 30000) {
    isHome();
    isHomeTime = millis();
    isHomeCount = 0;
    Serial.println(message);
  }

  if (millis() - postToThingspeak_time > 60000) {
    postToThingspeak();
    postToThingspeak_time = millis();
    Serial.println("postToThingspeak");
  }

  if (millis() - rolloverTime > 85000000) {
    timeRollover();
    rolloverTime = millis();
    Serial.println("Time counters dropped");
  }

  yield();
}

