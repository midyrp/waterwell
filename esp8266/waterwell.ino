#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "time.h"
#include "LittleFS.h"
#include "string.h"

//////////////////////////////////////////////////////////////
// Custom parameters
const char* wifi_ssid = "<SSIS>";
const char* wifi_psk = "<PSK>";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 0;
const int   sensor_depth = 0;
const char* webserver = "http://<SERVER>/datalogger.php";

//////////////////////////////////////////////////////////////
// Generic parameters
const char* MyName = "WaterWell";
const int trigPin = D6;
const int echoPin = D7;
//const char* ntpServer = "se.pool.ntp.org";
const char* ntpServer = "<NTP server IP>";
#define DeepSleepDuration 1800e6

//////////////////////////////////////////////////////////////
// Vars
const char* ProgVersion = "V0.0.8a";
const char* fileName = "backlog.dat";
ESP8266WiFiMulti WiFiMulti;
const size_t MaxStrLen = 1024;
const int procDelay = 500;
#define max_retry 10
#define retry_sleep 1000
#define SOUND_SPEED 0.034 //define sound speed in cm/uS
char* dataURL = (char *)malloc(MaxStrLen);


void WiFi_init(int newmode) {
  int cnt = 0;

  if ( newmode == 0 ) {
    Serial.println("Closing WiFi");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }
  Serial.println("Init WiFi");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(wifi_ssid, wifi_psk);
  while ((WiFiMulti.run() != WL_CONNECTED) && (cnt++ < max_retry) ) {
    Serial.println("Waiting for WiFi "  + String(cnt) + "/" + String(max_retry));
    delay(retry_sleep);
  }
  if (WiFiMulti.run() != WL_CONNECTED) {
    Serial.println("No WiFi available");
  } else {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Trying time sync");
    delay(2 * procDelay);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
}


long get_echo( ) {
  long echo_delay;

  Serial.println("Init ultrasound");
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  digitalWrite(trigPin, LOW); // Clear whatever is the case
  delayMicroseconds(2);
  Serial.println("Start ultrasound");
  digitalWrite(trigPin, HIGH); // Tigger us pulse
  delayMicroseconds(10);
  Serial.println("Stop ultrasound");
  digitalWrite(trigPin, LOW); // Stop us pulse
  echo_delay = pulseIn(echoPin, HIGH);
  Serial.print("Delay: ");
  Serial.println(echo_delay);
  return echo_delay;
}


int get_depth(long usDelay) {
  int depth;

  if ( usDelay > 0 ) {
    depth = int ( usDelay * SOUND_SPEED / 2 ) + sensor_depth ;
    Serial.print("depth: ");
    Serial.println(depth);
    return depth;
  } else {
    Serial.println("No echo");
    return -1;
  }
}


char * timestamp() {
  time_t now;
  char* timestamp = (char *)malloc(MaxStrLen);

  time(&now);
  strftime(timestamp, 40, "%Y%m%d-%H%M%S", localtime(&now));
  return (char*) timestamp ;
}


bool make_url( String dataKey, String dataValue ) {
  char tmpStr[500];

  if ( dataURL[0] == '\0' ) {
    Serial.println("URL init");
    strcat(dataURL, webserver);
    strcat(dataURL, "?");

    strcat(dataURL, "datasource=");
    strcat(dataURL, MyName );

    strcat(dataURL, "&version=");
    strcat(dataURL, ProgVersion );

    strcat(dataURL, "&timestamp=");
    strcat(dataURL, timestamp());
  }

  sprintf(tmpStr, "&%s=%s", dataKey, dataValue);
  strcat(dataURL, tmpStr );

  Serial.print("Made URL: ");
  Serial.println(dataURL);
  return true;
}


bool http_send( ) {
  WiFiClient client;
  HTTPClient http;
  int httpCode;

  Serial.print("Send URL: ");
  Serial.println(dataURL);
  if (http.begin(client, dataURL )) {
    httpCode = http.GET();
    if (httpCode == 200) {
      Serial.println("HTTP ok");
    } else {
      Serial.printf("[HTTP] GET error: %d\n", httpCode );
      return false;
    }
  } else {
    Serial.println("HTTP not started");
    return false;
  }
  http.end();
  return true;
}


bool store_url() {
  File filePntr;
  int cnt;

  Serial.println("Storing data");
  if (!LittleFS.begin()) {
    Serial.println("Data lost. Not stored on disk. Not mounted LittleFS");
    return false;
  }

  filePntr = LittleFS.open(fileName, "a");
  filePntr.println(dataURL);
  delay(procDelay);
  filePntr.close();

  delay(procDelay);
  cnt = 0;
  filePntr = LittleFS.open(fileName, "r");
  if (filePntr) {
    while (filePntr.available()) {
      (void) filePntr.readStringUntil('\n');
      cnt++;
    }
    Serial.print("Lines stored: ");
    Serial.println(cnt);
    filePntr.close();
  }
  return true;
}


bool send_stored() {
  File filePntr;
  String tmpStr;
  int x;

  Serial.println("Send stored data");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS not mounted. Not sending backlog.");
    return ( false );
  }
  if (!LittleFS.exists(fileName)) {
    Serial.println("No file with backlog.");
    return ( true );
  }
  filePntr = LittleFS.open(fileName, "r");
  if (!filePntr) {
    Serial.println("Could not open file.");
    return ( false );
  }
  while (filePntr.available()) {
    tmpStr = filePntr.readStringUntil('\n');
    dataURL = const_cast<char*>(tmpStr.c_str());
    // remove newline
    x = 0;
    while ( dataURL [x] != '\0' ) {
      x++;
    }
    if (x > 0) {
      dataURL [x - 1] = '\0';
    }

    Serial.print("Read backlog: ");
    Serial.println(dataURL);
    if ( ( dataURL[0] == 'h' ) && ( dataURL[1] == 't' ) && ( dataURL[2] == 't' ) && ( dataURL[3] == 'p' ) ) {
      if ( ! http_send() ) {
        Serial.println("Posting data failed");
        filePntr.close();
        return ( false );
      }
      // delay(procDelay);
    } else {
      Serial.println("Line does not start with http");
    }
  }
  filePntr.close();
  Serial.println( "Delete stored data" );
  LittleFS.remove(fileName);
  return true;
}


void setup() {
  int well_depth;
  long echotime;

  dataURL[0] = '\0';
  Serial.begin(115200);
  Serial.flush();
  delay(1000);
  Serial.println( ProgVersion );
  Serial.println("");

  WiFi_init(1);
  echotime = get_echo();
  make_url( "usDelay" , (String)echotime );
  well_depth = get_depth(echotime);
  make_url( "Water", (String)well_depth );
  if (! http_send( )) {
    store_url() ;
  } else {
    send_stored();
  }

  // Housekeeping and go to sleep
  WiFi_init(0);
  Serial.println("Goodnight !");
  ESP.deepSleep(DeepSleepDuration);
}


void loop() {

}
