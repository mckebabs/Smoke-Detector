# Smoke-Detector

The codes is used for the internet enabled smoke detector with additional functionality of motion detection/logging
Hardware used:

1) Smoke Detector: xxx
2) Controller: NodeMCU ESP8266
3) Custom circuit board for filtering the buzzer signal and triggering the alarm button via pnp transistor
4) Motion sensor: HC-S501
5) 5v->9v booster 

Features:

1) Online access. Youŗ Wifi SSID and Password must be entered in the "wifi" section

2) Remote control for triggering the alarm Test via Blynk app (iOS/Android)

3) Remote control of triggering the alarm Stop in case of an alarm via Blynk app

4) Time is synced from Blynk server and is used for the information about the last time controller has sent a message to the Blynk app

5) Data like Wifi signal strength, motion count for every 30 seconds, alarm detection and Days online is posted to Thingspeak 

6) If motion is detected, it will be reported to the Blynk app for 10 minutes, otherwise message "Nobody is at home" is displayed 

7) Alarm notification is send via Blynk PUSH notification, SMS and Email via IFTTT.com service(you have to create your own IFTTT recipe).

8) 3 LEDs for activity/status indications. Posting data to Thingspeak or sending data to Blynk is displayed via green led #1, motion detection via green led #2 and alarm notifications via all 3 leds

9) Fake "multi threading" orchestrating all processes

Video of how the device is functioning in real life can be seen here: https://xxx
