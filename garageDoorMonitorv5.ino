/*
    GarageMonitorApp - Monitoring of multiple garage doors Open/Close status to mail notifications and report data
    - Rev V5 modified by Tiago Medicci Serrano (tiago.medicci@gmail.com) in 2018.
    Copyright (C) 2015, Jay Moskowitz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Revisison History
// Rev V5 - Insert Blynk Library to enable graphic monitoring and e-mail notification
//        - Send garage door status to thingspeak through MQTT
//        - Adapted to Energia (Texas Instruments Arduino-like library) for CC3200
// Rev V4 - Insert Daylight Savings Time code as the Photon library
//          does not account for it. Adjust the GMT offset accordingly.
// Rev V3 - Correct mask in Timer wrap around logic - 10/21/2015
//          - 0xC00 0000 should have been 0xC000 0000
//              - caused LED timer to stop operating when mills()
//                  reached 0x8000 0000
//          - Update set_next_timeout() to insure when close to wrap around
//              time that the new timer is not set to 0 as this value is
//              sometimes used by the logic to indicate if a timer is
//              active or not.
//              - This caused the sync of the date/time from the cloud to
//                  stop and left the Date of alarms from being updated.
//          - Update step 8 in loop() which maintains the current time of day
//              - At clock wrap time, the function went into a loop and
//                  because of the _xC00 0000 error above, resulted in the 
//                  clock updates scheduled for a very very long time later
//                  when it came out of the loop. This error and the last one
//                  resulted in the Date/Time on text messages being locked
//                  to a specific Date/Time and not being updated. 
//          - All these errors were related to clock wrap around issues. 
//          - Please see the NOTE regarding clock wrap around just below 
//          
// Rev V2 - Included GNU license and released on Hacketer.io - 9/9/2015
// Rev V1 - Initial release on production Photon and in Operation since 6/1/2015

// This version of the code utilized two functions to deal with clock wrap around.
// These are set_next_timeout() and check_for_timeout(). Using these functions,
// one only needs to maintain a single unsigned long variable to represent the
// timeout time. The code makes sure that near clock wrap around time, it sets the
// next timeout so it occurs after the clock wraps back to 0 to insure that a timeout
// is not accidentally missed which could hang up a function. And alternative method
// of dealing with the clock wrap around entails with having the timer as a struct
// instead of a single unsigned long. The struct would contain the starting time when
// the timeout was requested and the interval of time you wish to wait. Then the 
// check_for_timeout function becomes:
//          if (start_time - millis()) < interval) return true; // Timed out
//          else return false;          // Didn't time out
// This requires carrying two values instead of the one being used. It works
// propertly because of unsigned arithmetic which results in this function waiting
// the proper timeout time. 
//
//  Monitoring of 3 garage doors via reed switches

//  LED Blink Patterns
//    2 sec on / 2 sec off      -  Idle loop
//    1 sec on / 1 sec off      -  Door is open - not in alarm state 
//    4 times per second        -  Door is open - in alarm state - Tweet has been sent
//

#ifdef ENERGIA
#include <WiFi.h>
#include <BlynkSimpleEnergiaWiFi.h>
#else
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiUdp.h>
#endif
#include <PubSubClient.h>
#include <TimeLib.h>
#include "debugPrint.h"


/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#define debug       1   // 1 for debug mode - progress diagnostics - with or without faster (debug) timers
#define debugTimers 1   // use debug timers in debug mode (else use normal timers)
#define debug_step  0   // ! to track each step of processing

#ifdef ENERGIA
#define LED             GREEN_LED       //Show status on green CC3200-LaunchXL board
#define SWITCH          PUSH2           //Read value from SW3 (hardware pull-down)
#else
#define LED             LED_BUILTIN
#define SWITCH          D2
#endif
#define GARAGE          SWITCH 
#define OPEN            HIGH
#define CLOSED          LOW
#define debounce        (unsigned long)50L    // 50ms debounce timeout


#define CR '\r'
#define LF '\n'

#define DEBUG_REMINDER 5L    // Debug time in minutes
#define NORMAL_REMINDER 15L  // Normal interval between reminders
#define DEBUG_CLK_UPDATE 5L   // update time of day clock from cloud every 5 minutes
#define NORMAL_CLK_UPDATE 60L // Once per hour

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "YourAuthTokenHere";

BlynkTimer AppTimer;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "YourWiFiSSIDHere";
char pass[] = "YourWiFiPasswordHere";

// Your ThingSpeak credentials through MQTT
char mqttUserName[] = "YourThingSpeakMQTTUsername"; // Can be any name.
char mqttPass[] = "YourThingSpeakMQTTAPIKey";       // Change this your MQTT API Key from Account > MyProfile.
char writeAPIKey[] = "YourThingSpeakChannelWriteAPIKey";  // Change to your channel Write API Key.
long channelID = YourThingSpeakChannelID;
short fieldID = 1;
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.

WiFiClient client;                          // Initialize the Wifi client library.
PubSubClient mqttClient(client);            // Initialize the PuBSubClient library.
const char* server = "mqtt.thingspeak.com"; // Server to connect toCharArray.

// NTP Servers:
static char ntpServerName[] = "a.ntp.br";

//Current timeZone
const int timeZone = -3;  //Local timezone

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();

void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

// Flash stored messages
const char f_open[] PROGMEM = "OPEN ";
const char f_closed[] PROGMEM = "CLOSED ";
const char f_at[] PROGMEM = " on ";
const char f_cr[] PROGMEM = "\n";
const char f_startup_msg[] PROGMEM = "\n************ Begin garage door switch test ************";
const char f_gpl1[] PROGMEM = "\nGarageMonitorApp - Version 5, Copyright(c) 2015, Jay Moskowitz"; 
const char f_gpl2[] PROGMEM = "\nThis program comes with ABSOLUTELY NO WARRANTY";
const char f_gpl3[] PROGMEM = "\nFor details refer to http://www.gnu.org/licenses/ ";  
const char f_sl[] PROGMEM = "\nStartup loop()";
const char f_dosc[] PROGMEM = "\nDoor opened - start clock ";
const char f_dcbasc[] PROGMEM = "\nDoor closed before alert - stop clock ";
const char f_mtasdtd[] PROGMEM = "\nMove to alert state due to door ";
const char f_odot[] PROGMEM = "\n--> One or more doors open text";
const char f_adct[] PROGMEM = "\n--> All doors closed text";
const char f_debug_clock[] PROGMEM = "----------> Clock is at ";
const char f_getTime[] PROGMEM = "\n-->Get Date/Time";
const char f_reminder[] PROGMEM = "\nSend Reminder Text";
const char f_step[] PROGMEM = "\nstep: ";

unsigned long proc_time, led_timer, req_time_update;
bool startup, led_state;
unsigned long open_clock, timeout, debounce_time, clk_time;
bool Door, last;
bool alert_state, time_to_alert, clock_running, clk_set;
unsigned long secondM, minuteM, hourM, dayM;
byte cur_hour, cur_minute, cur_second;
long main_count = 0;

char day_number = 0;
char month_number = 0;
char week_day_number = 0;

const char fmt1[] PROGMEM = "%d-%d:%d:%d.%ld";
const char fmt2[] PROGMEM = "%s %s %d, %02d:%02d:%02d";
const char fmt3[] PROGMEM = " main_count=%lx, millis()=%lx";

void show_step(byte step){
  if (debug_step){
    DebugPrintFO(f_step);
    Serial.println(step);
  }
}

void setup()
{
  // Debug console
  Serial.begin(9600);
  mqttClient.setServer(server, 1883);   // Set the MQTT broker details.
  
  pinMode(LED, OUTPUT);
  pinMode(SWITCH,INPUT);   // Normally in LOW state if nothing connected

  Door = CLOSED;            // initial door status before first reading of switched = closed
  last = CLOSED;            // For switch debounce logic
  open_clock = 0;
  clock_running = false;     

    if (debug) {
      // print noticeable startup message
      delay(1000);
      DebugPrintF(f_gpl1);	// Display GNU General Public License information
      DebugPrintF(f_gpl2);
      DebugPrintF(f_gpl3);
      DebugPrintF(f_startup_msg);
    }

  startup = true;
 
  alert_state = false;
  time_to_alert = false;

  secondM = 1000L;
  minuteM = 60L * secondM;
  hourM = 60L * minuteM;
  dayM = 24L * hourM;
  if (debug & debugTimers) {
    timeout = 3 * minuteM;
  }
  else timeout = 5 * minuteM; // how long to wait before sending out an alarm

  req_time_update = millis() + (30 * secondM);  // when to update clock - 30 secsafter startup
  
  led_state = HIGH;
  clk_set = false;


  Blynk.begin(auth, ssid, pass);
  Blynk.email("youremail@gmail.com", "[Monitor do Portão da Garagem] Sistema Reiniciado", "O monitor acabou de ser reiniciado");

  AppTimer.setInterval(10L, AppTimerEvent);

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  Blynk.run();
  AppTimer.run(); // Initiates BlynkTimer
  // Reconnect if MQTT client is not connected.
  if (!mqttClient.connected()) 
  {
    reconnect();
  }

  mqttClient.loop();   // Call the loop continuously to establish connection to the server.
}

void mqttpublish(int value) {  
  // Create data string to send to ThingSpeak
  String data = String(value, DEC);
  int length = data.length();
  char msgBuffer[length];
  data.toCharArray(msgBuffer,length+1);
  Serial.println(msgBuffer);
  
  // Create a topic string and publish data to ThingSpeak channel feed. 
  String topicString ="channels/" + String(channelID) + "/publish/fields/field" + String(fieldID) + "/" + String(writeAPIKey);
  length=topicString.length();
  char topicBuffer[length];
  topicString.toCharArray(topicBuffer,length+1);
 
  mqttClient.publish( topicBuffer, msgBuffer );
}

void reconnect() 
{
  char clientID[10];

  // Loop until we're reconnected
  while (!mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
        clientID[i] = alphanum[random(51)];
    }

    // Connect to the MQTT broker
    if (mqttClient.connect(clientID,mqttUserName,mqttPass)) 
    {
      Serial.println("connected");
    } else 
    {
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void AppTimerEvent()
{
  int i, j;
  static bool anyDoorOpen = false;
  static unsigned long print_time = 0, last_clk_time, submit_to_blynk = 0;
  static unsigned long reminder_timer;

  show_step(0);

  proc_time = millis();

  readSwitch();            // Read and debounce all switches and save current debounced switch status when ready
  if (startup) {
    // One time startup code 
    digitalWrite(LED, led_state);    // Turn On LED on board to show we are running
    led_timer = proc_time + 1000;    // 1 second blink timer

    getTime();                      // Start time of day clock
    if (debug) {
        DebugPrintF (f_sl);        // Serial.print("\nStartup loop()\n");
        Serial.println((char *)"");
    }
    proc_time = millis();
    readSwitch();                       // Read door switches for first half second 

    startup = false;    
  }
  if(!clk_set) getTime();       // If Real Time Clock did not start, try again
  
  show_step(1);

  // Start 5 minute clock (timeout period) for each door that has just opened
  if ( (alert_state == false) && (clock_running == false) && (Door == OPEN) ){
    set_next_timeout(&open_clock, proc_time, timeout);  // Start 5 minute clock 
    clock_running = true;
    if (debug) {
      DebugPrintFO(f_dosc);        // Serial.print ("\nDoor opened - start clock ");
      DebugPrintFO(f_at);          // " at "
      Serial.println (proc_time);
    }
  }
  
  show_step(2);  
  
  // Reset clock associated with any door that closes within the timeout period
  if ( (alert_state == false) && (clock_running) && (Door == CLOSED) ){
    open_clock = 0;        // Stop clock
    clock_running = false; 
    if (debug) {
      DebugPrintFO(f_dcbasc);   // Serial.print("\nDoor closed before alert - stop clock ");
    }     
  } 
  show_step(3);  
  
  // Check if it's time to alert
  // Have not already sent an alert, a door is open, check if open beyond timeout period
  if ( (alert_state == false) && clock_running && check_for_timeout(open_clock,proc_time) ) { 
      time_to_alert = true;      // at least one door open too long
      if (debug){
        DebugPrintFO(f_mtasdtd);  // Serial.print("\nMove to alert state due to door ");
        DebugPrintFO(f_at);      // Serial.print(" at ");
        Serial.println(millis());
      }
  }

  show_step(4);  
  
  // Send notification if time to alert then move into alert state
  if ( (alert_state == false) && time_to_alert) {
    if (debug) {
        DebugPrintF(f_odot);  // Serial.print("\n--> One door open tweet\n");
        Serial.println((char *)"");
    }
    alert_state = true;              // indicate we've alerted to door open
    time_to_alert = false;
    Blynk.email("youremail@gmail.com", "[Monitor do Portão da Garagem]", "O portão está aberto por mais de 5 minutos! (Cuidado, pessoal!)");
    // Set ReminderTimer to send status every 15 minutes (normal) if door remains open
    // Debug - Reminder every 5 minutes, Normal - Reminder every 15 minutes
    if (debug & debugTimers) set_next_timeout(&reminder_timer, proc_time, (unsigned long)(DEBUG_REMINDER * minuteM)); 
    else set_next_timeout(&reminder_timer, proc_time, (unsigned long)(NORMAL_REMINDER * minuteM)); // Remind every 15 minutes
    
  }
    
  show_step(5); 
  
  // Turn off alert state and send and all clear alert when door closed
  if ( (alert_state == true) &&
       ( (Door == CLOSED) ) )
    {
      if (debug) DebugPrintF(f_adct);

      alert_state = false;
      Blynk.email("youremail@gmail.com", "[Monitor do Portão da Garagem]", "O portão acabou de ser fechado (Ufa!)");

      //TODO: Send alert to BLYNK and THINGSPEAK

      for (j = 0; j < 3; j++) {
        open_clock = 0;        // clear door open timers
        clock_running = false;
      }
      reminder_timer = 0;  // No more reminders necessary
    }
    
  show_step(6);  
    
  if (reminder_timer && check_for_timeout(reminder_timer,proc_time)){
    // Reminder has timed out. Time to send status again
    // First set another reminder period
    if (debug & debugTimers) {
      set_next_timeout(&reminder_timer, proc_time, (unsigned long)(DEBUG_REMINDER * minuteM));
      DebugPrintF(f_reminder);
      Serial.println((char *)"");
    } 
    else set_next_timeout(&reminder_timer, proc_time, (unsigned long)(NORMAL_REMINDER * minuteM)); 
    Blynk.email("youremail@gmail.com", "[Monitor do Portão da Garagem]", "O portão CONTINUA aberto! (Chequem urgentemente, por favor...)");
    //TODO: Send alert to BLYNK and THINGSPEAK
  }

  show_step(7);  
  
  // Update the time of day once each hour
  if (req_time_update && check_for_timeout(req_time_update,proc_time)){ // time to update clock from Internet
    getTime();
    if (debug & debugTimers)
         set_next_timeout(&req_time_update, proc_time, (unsigned long)(DEBUG_CLK_UPDATE * minuteM));  // request time again in 5 minute
    else set_next_timeout(&req_time_update, proc_time, (unsigned long)(NORMAL_CLK_UPDATE * minuteM));  // update clock once per hour  
  }
  
  show_step(8);

  // Maintain current time of day once it is read off the Internet
  // proc_time could be incorrect because the application hung up for several minutes
  // resetting the WiFi hardware. But the following loop will account for all
  // the time that has passed and correctly update the time of day clock.
  unsigned long last_timeout;
  if (clk_set && check_for_timeout(clk_time,proc_time)){  // 1 or more seconds have passed since last clock update
    while (clk_time <= proc_time){
      last_clk_time = clk_time;
      cur_second += 1;
      if (cur_second >= 60) {cur_second = 0; cur_minute += 1;}
      if (cur_minute >= 60) {cur_minute = 0; cur_hour += 1;}
      if (cur_hour >= 24) {cur_minute = 0; cur_hour = 0;}
      set_next_timeout(&clk_time, clk_time, (unsigned long)1000L);          // add the 1 second just accounted for - loop until clock is adjusted
      // Check if last clk_time was a large value but the new clk_time is small
      if ((last_clk_time & 0x80000000) && (clk_time && 0xC0000000 == 0)){
         // clock is about to wrap around. Stop updating the time of
         // day until the wrap around which will happen within 1 minute
         // The clock will not be update again until the millis() wrap around
         // It will then incorrectly update the HH:MM:SS because it will have
         // missed about 1 minute of updates. But the next call to get_time()
         // to read the clock from the cloud, will correct the time of day.
         // The clock will be off by 1 minute for 1 hour or less (until the
         // time of day update). 
         break;
      }
    }
  }
  show_step(9);   
  
  // ---- LED Light Patterns will reflect the current state of processing
  
  // Blink Blue LED in various patterns to indicate status
  // 4 sec on / 4 sec off - idle mode
  // 1 sec on / 1 sec off - door is open
  // 4x/ sec - door is open and e-mail alert sent - waiting for it to close to send another alert
  if (check_for_timeout(led_timer,proc_time)){
    led_state = (led_state == HIGH)? LOW : HIGH;    // change state of the Yellow LED
    digitalWrite(LED,led_state);
    // Determine when next change of state is to occur
    if ( Door == OPEN ){
      // Door is open
      if (alert_state) set_next_timeout(&led_timer, millis(), (unsigned long)125L);      // Blink 4x a second in alert state waiting for door to close
      else set_next_timeout(&led_timer, millis(), (unsigned long)1000L);  // Blink once per second while door open for short time period
    }
    else {
      anyDoorOpen = false;           // All doors were just closed
      set_next_timeout(&led_timer, millis(), (unsigned long)4000L);    // Blink slowly to show program is running
    }
  }  
  show_step(10);
  
  if ((anyDoorOpen == false) && (Door == OPEN)) {
    anyDoorOpen = true;
    led_timer = 0;
    led_state = LOW;          // immediately display open door state
  }
  if ((anyDoorOpen == true) && (Door == CLOSED)) {
    anyDoorOpen = false;
    led_timer = 0;
    led_state = LOW;          // immediately display open door state
  } 
  main_count++;
  
  show_step(11);
  
  // insure proc_time is up to date. Could be wrong if we got hung up in the TCP
  // Check timeout to write door status to ThingSpeak every 15s  
  proc_time = millis();  
  if (check_for_timeout(print_time,proc_time)){
    set_next_timeout(&print_time, proc_time, (unsigned long)15000L);
    if( Door == CLOSED ){
      mqttpublish(0);
    }else{
      mqttpublish(1);
    }
    if (debug){
      DebugPrintFO(f_debug_clock);   // Clock is at ##:##:##
      if (clk_set){
        if(cur_hour <= 9) Serial.print("0");
        Serial.print(int(cur_hour));
        Serial.print(":");
        if(cur_minute <= 9) Serial.print("0");
        Serial.print(int(cur_minute));
        Serial.print(":");
        if(cur_second <=9) Serial.print("0");
        Serial.print(int(cur_second));

        char bfr[90], fmt[68];

        strcpy_P(fmt,fmt3);
        sprintf(bfr,fmt,main_count,millis());        
        Serial.println(bfr);
      }
      else Serial.println((char *)"");
    }
  }
  show_step(12);

  // insure proc_time is up to date. Could be wrong if we got hung up in the TCP
  // Check timeout to write door status to virtual pin on Blynk every 200ms.
  proc_time = millis();  
  if (check_for_timeout(submit_to_blynk,proc_time)){
    set_next_timeout(&submit_to_blynk, proc_time, (unsigned long)200L);
    // report door status at Blynk and at ThingSpeak
    if( Door == CLOSED ){
      Blynk.virtualWrite(V0, CLOSED);
    }else{
      Blynk.virtualWrite(V0, OPEN);
    }
  }
  show_step(13);
}

// Read reed switches and debounce input
void readSwitch(){
  int i;
  bool swd;

  swd = digitalRead(GARAGE);   // Set HIGH (circuit closed) or LOW (circuit open)

  if (swd != last){
    last = swd;              // save current switch value (high or low)
    set_next_timeout(&debounce_time, proc_time, debounce);  // time before reading considered stable
  }

  if (check_for_timeout(debounce_time,proc_time)){  // timer has timed out
    Door = swd;             // switch has been debounced. Same setting for some period of time
  }
} 

// Parse/Analyze data returned from remote time server
void getTime(){
  
  if(year()<=1970) return; // This means the RTC has not yet started
  //TODO: Check DST function for Brazil
  /*
  bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
  Time.zone(daylightSavings? -4 : -5);
  */
  
  cur_hour = hour();
  cur_minute = minute();
  cur_second = second();
  day_number = day();       // 1 - 31
  month_number = month();   // 1 -12
  week_day_number = weekday();     // 1 = Sunday
  
  if (debug) {
      DebugPrintFO(f_getTime);
      Serial.println((char *)"");
  }
  clk_set = true;            // once clock is set at least once, the system will maintain the time
  // Set clk_time to count the number of seconds from now and adjust the current time accordingly
  // The set_next_timeout() function will consider the clock as getting ready to wrap around
  // during the last one minute before wrap around time every 49.7102696 days
  // (49 days, 17 hours, 2 minutes, 47.295 seconds)
  set_next_timeout(&clk_time, millis(),1000L);       // Update local time in 1 second      
}

// check to see if timeout has occurred
bool check_for_timeout(unsigned long timeout_time, unsigned long cur_time){
  if ( ( (timeout_time & 0xC0000000) == 0) &&    // timeout time a very low value
         (cur_time & 0x80000000) )              // current time is a very large value
    return false;                               // No timeout yet

  if (timeout_time <= cur_time) return true;     // Timeout has occurred
  else return false;                            // No timeout yet
}
  
  
void set_next_timeout(unsigned long *timer, unsigned long baseclock,  unsigned long increment){
  *timer = baseclock + increment;      // timeout for next event
  if (!*timer) *timer = 1;          // Add 1ms to timeout to avoid timeout at 0 
  
  // if the new timeout is within 1 minute of wrapping around, add another minute to insure it
  // wraps around as it is easy to miss the timeout check near the clock wrap around point
  if (*timer > ((unsigned long)0xffffffff - (unsigned long)minuteM)) *timer += minuteM;

  return;
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}


void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// Check for Daylight Savings Time
bool IsDST(int dayOfMonth, int month, int dayOfWeek)
{
  if (month < 3 || month > 11)
  {
    return true;
  }
  if (month > 3 && month < 11)
  {
    return false;
  }
  int previousSunday = dayOfMonth - (dayOfWeek - 1);
  if (month == 3)
  {
    return previousSunday >= 8;
  }
  return previousSunday <= 0;
}
