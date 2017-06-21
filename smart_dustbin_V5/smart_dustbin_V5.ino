#include "HX711.h"
#include "ESP8266WiFi.h"
#include <NTPtimeESP.h> //Include NTP library to get NTP time
#include <WiFiManager.h>
//Library for OLED
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`


//--------NTP Time------------------------
#define DEBUG_ON
NTPtime NTPUK("uk.pool.ntp.org"); // Choose server pool as from UK
strDateTime dateTime;   //Structure to store date and time info
String JSONdateTime;

//--------WiFi Sleep-------------
const int sleepTimeSeconds = 24 * 60 * 60;

//--------Ultrasonic Range Sensor-------------
#define TRIGGER 13
#define ECHO 15
float duration, distance, volume;

//------Custom Dustbin Dimensions-----
char confLength[20] = "";
char confWidth[20] = "";
char confHeight[20] = "";

//--------Load Cell---------------------------
HX711 scale;
float weight;

//-----FoodAppee API Settings-----
WiFiClient client;                     //Create client to connect to specified IP and port defined in .connect()
const int postingInterval = 10 * 1000; //Post data every 10 seconds
const char *server = "foodappee.azurewebsites.net";
String UserId = "7";
String postData;

//----------OLED----------
SSD1306  display(0x3c, 4, 14);
String testing = "OK";

//---------RGB LED--------------
int redPin = 1;
int greenPin = 3;
int bluePin = 12;

//uncomment this line if using a Common Anode LED
//#define COMMON_ANODE

//-----Functions-----
String zeroExtend(String time);
void getDateTime();
void printMetrics();
void postJSONData();
void formatJSONString();

//###########################################
//               SETUP                      #
//###########################################
void setup()
{

  //--------Ultrasonic Range Sensor-------------
  Serial.begin(9600);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while (!Serial) { }
  Serial.println("Device Started");
  Serial.println("-------------------------------------");

  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(0, OUTPUT);

  //-----RGB LED-----
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  //--------Load Cell---------------------------
  Serial.println("Initializing the scale"); // parameter "gain" is ommited; the default value 128 is used by the library
  scale.begin(0, 2);                       //DOUT,SCK

  scale.set_scale(-455.f); // this value is obtained by calibrating the scale with known weights;
  scale.tare();

  //---------------------Setup WiFi and Serial-----------------------------------------
  WiFiManager wifiManager;

  // Adding an additional config on the WIFI manager webpage for the API Key and Channel ID
  WiFiManagerParameter customLenght("confLength", "Length", confLength, 20);
  WiFiManagerParameter customWidth("confWidth", "Width", confWidth, 20);
  WiFiManagerParameter customHeight("confHeight", "Height", confHeight, 20);
  wifiManager.addParameter(&customLenght);
  wifiManager.addParameter(&customWidth);
  wifiManager.addParameter(&customHeight);

  wifiManager.autoConnect("Smart Dustbin");

  strcpy(confLength, customLenght.getValue());
  strcpy(confWidth, customWidth.getValue());
  strcpy(confHeight, customHeight.getValue());

  Serial.println("The configurations are: ");
  //  if (confLength!=NULL) {
  //    Serial.println(String(confLength) + "\t" + String(confWidth) + "\t" + String(confHeight));
  //  }

  //-----------------------------------------------------------------------------------------------------
  //---------------------Get NTP Time when starting up---------------------
  getDateTime();

  //--------Ultrasonic Range Sensor-------------
  digitalWrite(TRIGGER, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration / 2) / 29.1;

  Serial.print(distance);   //Print distance in cm
  if (distance <= 5 ) {
    setColor(255, 0, 0);  // red
  } else {
    setColor(0, 0, 255);  // blue
  }
  Serial.println("Centimeter:");

  volume = 25 * 40 * (50 - distance);
  delay(1000);

  //--------Load Cell---------------------------
  Serial.print("one reading:\t");
  Serial.print(scale.get_units(), 2);
  Serial.print("\t| average:\t");
  weight = scale.get_units(10);
  Serial.println(weight, 2);

  scale.power_down(); // put the ADC in sleep mode
  delay(1000);
  scale.power_up();

  //-------OLED Initialisation-------
  display.init();
  display.setContrast(255);
  printMetrics();

  //----------POST data to server----------
  formatJSONString();
  postJSONData();

  //--------------------------------------------------------------------------------------------------------------

  Serial.println("ESP8266 Going into Deep Slumber for: " + String(sleepTimeSeconds) + "Seconds");
  ESP.deepSleep(sleepTimeSeconds * 1000000);    //GPIO16 to RST pin for this to work
//  delay(5000);

}

//###########################################
//                LOOP                      #
//###########################################

void loop()
{

}

//###########################################
//                Functions                 #
//###########################################

String zeroExtend(String time) {
  if (time.length() == 1) {
    return "0" + time;
  } else {
    return time;
  }
}

void getDateTime() {
  strDateTime dateTime;   //Structure to store date and time info
  Serial.println("Getting NTP Data");
  dateTime = NTPUK.getNTPtime(1.0, 0);
  while (!dateTime.valid) {
    Serial.print("Error reading NTP data");
    delay(100);
    dateTime = NTPUK.getNTPtime(1.0, 0);
  }
  Serial.println();
  byte iHour = dateTime.hour;
  String actualHour = zeroExtend(String(iHour));
  byte iMinute = dateTime.minute;
  String actualMinute = zeroExtend(String(iMinute));
  byte iSecond = dateTime.second;
  String actualSecond = zeroExtend(String(iSecond));
  int actualYear = dateTime.year;
  byte actualMonth = dateTime.month;
  byte actualDay = dateTime.day;
  byte actualDayofWeek = dateTime.dayofWeek;

  //  JSONdateTime = "\"" + String(dateTime.year) + "-" + String(dateTime.month) + "-" + String(dateTime.day) + "T" + String(dateTime.hour) + ":" + String(dateTime.minute) + ":" + String(dateTime.second) + "\"";
  JSONdateTime = "\"" + String(actualYear) + "-" + String(actualMonth) + "-" + String(actualDay) + "T" + actualHour + ":" + actualMinute + ":" + actualSecond + "\"";
  Serial.println(JSONdateTime);

}

void printMetrics() {
  // Initialize the log buffer
  // allocate memory to store 8 lines of text and 30 chars per line.
  display.setLogBuffer(5, 30);
  String tmpWeight = String(weight) + "g";
  String tmpVolume = String(volume) + "cm3";

  // Some test data
  const char* test[] = {
    JSONdateTime.c_str(), "Weight: ", tmpWeight.c_str(), "Volume: ", tmpVolume.c_str()
  };
  //Use .c_str to convert strings to char arrays

  for (uint8_t i = 0; i < 5; i++) {
    display.clear();
    // Print to the screen
    display.println(test[i]);
    // Draw it to the internal screen buffer
    display.drawLogBuffer(0, 0);
    // Display it on the screen
    display.display();
    delay(500);
  }
}


void formatJSONString()
{
  //JSON Format
  postData = "{\"UserId\":" + UserId + ",";
  postData += "\"Timestamp\":" + JSONdateTime + ",";
  postData += "\"Weight\":" + String(weight) + "}";
  Serial.println(postData);
}

void postJSONData()
{
  //----------FoodAppee API POST Data-----
  if (client.connect(server, 80))
  {
    client.println("POST /postWaste HTTP/1.1");
    client.println("Host: foodappee.azurewebsites.net");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.print(postData.length());
    client.print("\n\n");
    client.println(postData);
    client.print("\n\n");

    Serial.println("POST /postWaste HTTP/1.1");
    Serial.println("Host: foodappee.azurewebsites.net");
    Serial.println("Content-Type: application/json");
    Serial.print("Content-Length: ");
    Serial.print(postData.length());
    Serial.print("\n\n");
    Serial.println(postData);
    Serial.print("\n\n");
  }
  client.stop();          //Stop connecting to Thingspeak server
  delay(postingInterval); // wait and then post again
}

void setColor(int red, int green, int blue)
{
  #ifdef COMMON_ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  #endif
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);  
}
