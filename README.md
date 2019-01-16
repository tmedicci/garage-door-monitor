# Garage Door Monitor

Afraid of letting your garage door opened when you are out? This project monitors your garage door, report status on Blynk.cc cloud and ThingSpeak and sends you a notification through an e-mail address when it is opened for some minutes. This project is inspired in Jay Moskowitz post on [hackster.io](https://www.hackster.io/team-wireless-marvels-inc/garage-door-status-alert-to-sms-text-bc52f0) (take a look at it!).

## Based on Arduino!

Blynk.cc is a library that ease you to create an app and control your hardware directly on your smartphone. Arduino is the most used interface with Blynk! This project uses Arduino, Blynk library and MQTT component to send data to Thingspeak. 

### Choose your board!

This project uses a CC3200 launchpad and Energia (an Arduino-based interface with Texas Instruments launchpads). However, it is totally compliant with ESP8266-based boards. 

### Blynk.cc

Blynk is an extraordinary tool for making it easier to develop an application. Take a look at [getting started](https://www.blynk.cc/getting-started/). We are going to use mail notification feature and develop an app to monitor current door status. Download Blynk library and install it at Arduino IDE or Energia IDE.

### Why not Thingspeak?

Why not report door status on Thingspeak and make it easier to people who lives with you see current ans historical data? Create a channel on thingspeak and report data using MQTT API Key and Write API Key.

## How to use

### Edit the following sections of the code:

* `char auth[] = "YourAuthTokenHere";`, (from Blynk!).
* `char ssid[] = "YourWiFiSSIDHere";` and `char pass[] = "YourWiFiPasswordHere";` (a little bit self-explaining).
* `char mqttPass[] = "YourThingSpeakMQTTAPIKey";`, MQTT API Key from Thingspeak Account > MyProfile.
* `char writeAPIKey[] = "YourThingSpeakChannelWriteAPIKey";`, your channel Write API Key.
* `long channelID = 12345;`, your channel ID on Thingspeak.
* `short fieldID = 1;`, the field you want to send data for on Thingspeak.
* `youremail@gmail.com"`, whenever it appears along the code. It will send you an e-mail notification if door is opened for a while.

### Create an app in Blynk

Create an app containing the following widgets:
* Superchart, to monitor current garage door status on V1 (virtual pin 1, read about it at (blynk.cc))
* Email, to send e-mail notification everytime door is opened for more than some minutes (also defined in code).

### Check Pins Logic

This project uses a CC3200 launchpad which contains a embedded switch (SW2) with a hardware pull-down. When garage door is opened, a relay from door controller closes. These relays pins pull-up `SWITCH` input pin, indicating that door is opened. Other boards and garage door controller may require another strategy to indicate that door is oponed and this project should be modified according to it.

### Upload your code, contribute and be happy!

I hope it would be useful!
