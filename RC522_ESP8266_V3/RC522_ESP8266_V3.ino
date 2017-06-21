/* Pin Layouts:
  -------------------------------------------------------------------------------------------------
              MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino      ESP8266
              Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
  Signal      Pin          Pin           Pin       Pin        Pin              Pin
  -------------------------------------------------------------------------------------------------
  RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST          2
  SPI SS      SDA(SS)      10            53        D10        10               10           4
  SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16           13
  SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14           12
  SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15           14

  MFRC522 Library was imported from: https://github.com/ljos/MFRC522
*/

#include <SPI.h>      // Used for communication via SPI with the Module
#include <MFRC522B.h> // Include of the RC522 Library

#include <NTPtimeESP.h>  // Include NTP Time Library to get NTP Time
#include "ESP8266WiFi.h" //include wifi library
#include <WiFiManager.h>  //Include library to enable WiFi Direct WiFi configuration
#include <vector>;

//========================NTP TIme Initialisation Variables==========================
NTPtime NTPUK("uk.pool.ntp.org"); // Choose server pool as required
strDateTime dateTime;
String JSONdateTime;

//========================RC522 Initialisation Variables==============================
#define SDAPIN 4   // RFID Module SDA Pin is connected to the UNO 10 Pin
#define RESETPIN 2 // RFID Module RST Pin is connected to the UNO 8 Pin

byte foundTag;         // Variable used to check if Tag was found
byte readTag;          // Variable used to store anti-collision value to read Tag information
byte tagData[MAX_LEN]; // Variable used to store Full Tag Data
byte TagID[4];         // Variable used to store only Tag Serial Number
int matchingTagID;

//---------------------Hard coded Items corresponding to tag ID------------------
byte Chicken[4] = {0xB0, 0xD3, 0x8C, 0x50}; //Put 5??
byte Cabbage[4] = {0x60, 0xA0, 0x83, 0x4D};
byte Bread[4] = {0x82, 0xA4, 0x34, 0x5B};
byte Milk[4] = {0x32, 0xB5, 0x34, 0x5B};

struct Grocery
{
  int ProductId;
  int UserId;
  String ProductName;
  int Weight;
  float Price;
  String Purchased;
  String Expiry;
  int Priority;
  int AmountRemaining;
  bool inFridge;
};

Grocery Database[4];

MFRC522 hfRFID(SDAPIN, RESETPIN); // Init of the library using the UNO pins declared above

//===========================FoodAppee Setup=====================
const char *server = "foodappee.azurewebsites.net";
WiFiClient client;
const int postingInterval = 1 * 1000; // post data every 20 seconds
String UserId = "7";
String postData;

//---------------------Functions-----------------------------------
String zeroExtend(String time);
void getDateTime();
void initialiseData();
int findTagID();
void formatJSONString();
void postJSONData();

//------------------------------------------------------//
//                          Setup                       //
//------------------------------------------------------//
void setup()
{
  //======================NTP and WiFi Setup=====================
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booted");
  Serial.println("Connecting to Wi-Fi");

  WiFiManager wifiManager;
  wifiManager.autoConnect("Smart Fridge");

  Serial.println("WiFi connected");

  //===========================RC522 Setup=====================
  SPI.begin();
  Serial.begin(115200);
  Serial.println("Looking for RFID Reader");
  hfRFID.begin();
  byte version = hfRFID.getFirmwareVersion(); // Variable to store Firmware version of the Module

  // If can't find an RFID Module
  if (!version)
  {
    Serial.print("Didn't find RC522 board.");
    while (1)
      ; //Wait until a RFID Module is found
  }

  // If found, print the information about the RFID Module
  Serial.print("Found chip RC522 ");
  Serial.print("Firmware version: 0x");
  Serial.println(version, HEX);
  Serial.println();

  initialiseData();
  Serial.println("Database is loaded into application");

  Serial.println("The Scanned Item is: ");
}

//------------------------------------------------------//
//                          Loop                        //
//------------------------------------------------------//
void loop()
{

  int itemTag = 0;

  // ------------1. Check to see if a Tag was detected and print out serial number----------------
  // If yes, then the variable foundTag will contain "MI_OK"
  foundTag = hfRFID.requestTag(MF1_REQIDL, tagData); //request for tag and stores serial in tagData

  if (foundTag == MI_OK)
  {
    //--------------------Get NTP Time for timestamp--------------------
    getDateTime();

    //---------------------------Getting Grocery RFID ID----------------------------
    delay(100);

    // Get anti-collision value to properly read information from the Tag using tagData(serial number)
    readTag = hfRFID.antiCollision(tagData);

    //SYNTAX: memcpy ( void * destination, const void * source, size_t num ) 4 bytes in this case
    memcpy(TagID, tagData, 4); // Write the Tag information in the TagID array

    //Store Tag data into Array to compare later
    scannedTags.push_back(TagID);
    
    Serial.println("Tag detected.");
    Serial.print("Tag ID of item:  ");
    for (int i = 0; i < 4; i++)
    { // Loop to print serial number to serial monitor
      Serial.print(TagID[i], HEX); //print in hexadecimal format instead of bin/decimal
      Serial.print(", ");
    }
    Serial.println();

    //--------------------------Identifying Grocery by ID--------------------------
    findTagID();
    formatJSONString();
    postJSONData();
  }
}

//-----------------------------------------------------------//
//                          Functions                        //
//-----------------------------------------------------------//

String zeroExtend(String time) {
  if (time.length() == 1) {
    return "0" + time;
  } else {
    return time;
  }
}

void getDateTime() {
  dateTime = NTPUK.getNTPtime(1.0, 0); //Get NTP time of UK
  while (!dateTime.valid) {
    Serial.println("failed to get NTP data...");
    dateTime = NTPUK.getNTPtime(1.0, 0);
    Serial.println("Retrying to get NTP data...");
  }
  Serial.println("Obtained NTP successfully");
  NTPUK.printDateTime(dateTime);

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
  Serial.println("The NTP Date and Time is: " + JSONdateTime);

}

void initialiseData()
{
  //Chicken
  Database[0].ProductId = 51;
  Database[0].UserId = 7;
  Database[0].ProductName = "Chicken";
  Database[0].Weight = 500;
  Database[0].Price = 4.60;
  Database[0].Purchased = "";
  Database[0].Expiry = "2017-06-20T09:12:46";
  Database[0].Priority = 1;
  Database[0].AmountRemaining = 200;
  Database[0].inFridge = true;

  //Cabbage
  Database[1].ProductId = 52;
  Database[1].UserId = 7;
  Database[1].ProductName = "Cabbage";
  Database[1].Weight = 200;
  Database[1].Price = 4.60;
  Database[1].Purchased = "";
  Database[1].Expiry = "2017-06-16T09:12:46";
  Database[1].Priority = 2;
  Database[1].AmountRemaining = 100;
  Database[1].inFridge = true;

  //Bread
  Database[2].ProductId = 53;
  Database[2].UserId = 7;
  Database[2].ProductName = "Bread";
  Database[2].Weight = 600;
  Database[2].Price = 4.60;
  Database[2].Purchased = "";
  Database[2].Expiry = "2017-06-14T09:12:46";
  Database[2].Priority = 1;
  Database[2].AmountRemaining = 100;
  Database[2].inFridge = true;

  //Milk
  Database[3].ProductId = 54;
  Database[3].UserId = 7;
  Database[3].ProductName = "Milk";
  Database[3].Weight = 1000;
  Database[3].Price = 1.20;
  Database[3].Purchased = "";
  Database[3].Expiry = "2017-06-19T09:12:46";
  Database[3].Priority = 1;
  Database[3].AmountRemaining = 500;
  Database[3].inFridge = true;
}

int findTagID()
{
  if (Chicken[0] == TagID[0] && Chicken[1] == TagID[1] && Chicken[2] == TagID[2] && Chicken[3] == TagID[3])
  {
    matchingTagID = 0;
  }
  else if (Cabbage[0] == TagID[0] && Cabbage[1] == TagID[1] && Cabbage[2] == TagID[2] && Cabbage[3] == TagID[3])
  {
    matchingTagID = 1;
  }
  else if (Bread[0] == TagID[0] && Bread[1] == TagID[1] && Bread[2] == TagID[2] && Bread[3] == TagID[3])
  {
    matchingTagID = 2;
  }
  else if (Milk[0] == TagID[0] && Milk[1] == TagID[1] && Milk[2] == TagID[2] && Milk[3] == TagID[3])
  {
    matchingTagID = 3;
  }
  else
  {
    Serial.print("Item does not exist");
  }
}

void formatJSONString()
{
  //JSON Format
  postData = "{\"UserId\":" + UserId + ",";
  postData += "\"FoodItems\":[{";
  postData += "\"ProductId\":" + String(Database[matchingTagID].ProductId) + ",";
  postData += "\"UserId\":" + String(Database[matchingTagID].UserId) + ",";
  postData += "\"ProductName\":\"" + String(Database[matchingTagID].ProductName) + "\",";
  postData += "\"Weight\":" + String(Database[matchingTagID].Weight) + ",";
  postData += "\"Price\":" + String(Database[matchingTagID].Price) + ",";
  postData += "\"Purchased\":" + JSONdateTime + ",";
  postData += "\"Expiry\":\"" + String(Database[matchingTagID].Expiry) + "\",";
  postData += "\"Priority\":" + String(Database[matchingTagID].Priority) + ",";
  postData += "\"AmountRemaining\":" + String(Database[matchingTagID].AmountRemaining) + ",";
  postData += "\"inFridge\":" + String(Database[matchingTagID].inFridge) + "}]}";
  Serial.println(postData);
}

void postJSONData()
{
  if (client.connect(server, 80))
  {
    Serial.println("Sending data to Database");
    //................Format that FoodAppee uses...................
    client.println("POST /AddItems HTTP/1.1");
    client.println("Host: foodappee.azurewebsites.net");
    client.println("Connection: close");
    client.println("Content-Type: application/json"); //application/json
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println("\n\n");
    client.println(postData);
    client.println("\n\n");
  }
  else
  {
    Serial.println("Connection Failed");
  }
  Serial.println("Data Posted up to database");
  Serial.println();
  Serial.println();
  client.stop(); //Stop connecting to Thingspeak server
}

