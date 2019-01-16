#ifdef CC3200
#include <Energia.h>
#else
#include <Arduino.h>
#endif
#include <stdio.h>
#include "debugPrint.h"

#define TIME_OF_DAY_AVAILABLE 1

#if TIME_OF_DAY_AVAILABLE
	extern unsigned char cur_hour, cur_minute, cur_second;
	extern long mainCount;
#endif

const char debug_fmt1[] = ", %d-%d:%d:%d.%03ld ";
const char debug_fmt2[] = ", %2d:%02d:%02d ";

void PrintTime(bool running_time_only){
  unsigned long second, minute, hour, day;
  unsigned long currtime;
  unsigned int seconds, minutes, hours, days;
  unsigned long fraction;
  char fmt[30];
  char timeStamp[40];

  second = 1000L;
  minute = 60L * second;
  hour = 60L * minute;
  day = 24L * hour;

  currtime = millis();
  days = currtime / day;
  currtime -= (days * day);
  hours = currtime / hour;
  currtime -= (hours * hour);
  minutes = currtime / minute;
  currtime -= (minutes * minute);
  seconds = currtime / second;
  currtime -= (seconds * second);
  fraction = currtime;

  strcpy_P(fmt,debug_fmt1);
  sprintf(timeStamp,fmt,days,hours,minutes,seconds,(long)fraction);
  Serial.print(timeStamp);

#if TIME_OF_DAY_AVAILABLE

  if (!running_time_only)
    {
	  strcpy_P(fmt,debug_fmt2);	  
	  sprintf(timeStamp,fmt,
			cur_hour, cur_minute, cur_second);	
	  Serial.print(timeStamp);
    }
#endif

  Serial.println((char *)"");
}

void DebugPrint(char *msg){
	Serial.print (msg);
	PrintTime(true);
}

void DebugPrintO(char *msg){
	Serial.print(msg);
}

void DebugPrintNO(int n){
	Serial.print(n);
}

void DebugPrintHexO(int n){
	Serial.print(n,HEX);
}

void DebugPrintHex(int n){
	Serial.print(n,HEX);
	PrintTime(true);
}

void DebugPrintNumber(int number){
	Serial.print(number);
	PrintTime(true);
}

void debug_print_flash(const char *s)
{
	char c;
	while ((c = (char)(pgm_read_byte(s++)) )){
	  Serial.print(c);
	}
} 	

void DebugPrintF (const char *s)
{
	debug_print_flash(s);
	PrintTime(true);
}

void DebugPrintFO (const char *s)
{
	debug_print_flash(s);
}

// Print time of day in addition to running time
void DebugPrintFx (const char *s)
{
	debug_print_flash(s);
	PrintTime(false);
}

