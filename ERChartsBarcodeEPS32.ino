//#include <WiFi.h>
#include <ESPmDNS.h>
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
#include "ota.h"
#include "SoftwareSerial.h"
#include <FirebaseESP32.h>


#include <SPI.h>
#include <Time.h>
#include <FS.h>
#include <stdio.h>



#define inPin 27
#define rxPin 14
#define txPin 12
#define blueLed 13
#define redLed 26

#define ESPNAME  "CardioERTicketPrinter"
String path  = "/ERCharts/locations";
String basePath  = "/ERCharts/new";

const char* ssid = "ErCharts";
const char* password = "22Sharks";
const char* ntpServerName = "us.pool.ntp.org";
SoftwareSerial swUSB;

#include <FirebaseESP32.h>
#define FIREBASE_HOST "cardioer-cb630.firebaseio.com"
#define FIREBASE_AUTH "MKDVScVnRofBVJeE7iw0bgApOgTJYSnn79FqmZVM"

//Define Firebase Data object
FirebaseData fbdo;
FirebaseJson json;

WiFiClient client;
IPAddress timeServerIP; // time.nist.gov NTP server address


int val = 0;     // variable for reading the pin status
int ptNumber = 0;    // variable for reading the pin status
int toDay = 0;
int toMonth = 0;
int toYear = 0;
int nowTime;
int aliveCount = 0;
int deadCount = 0;
int streamError = 0;
char ipAdd[15];
String wifiConn = ssid;
unsigned int aliveSyncTime = 60;
unsigned long aliveLastUpdate = 0;
const long timeZoneOffset = -14400L;
unsigned int ntpSyncTime = 86400;
unsigned int localPort = 2390;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
WiFiUDP Udp;
unsigned long ntpLastUpdate = 0;
time_t prevDisplay = 0;
// LongPress
long buttonTimer = 0;
long longPressTime = 2000;
boolean buttonActive = false;
boolean longPressActive = false;
String location="0", area="0", areaTag2 = "", uniq, locationTag = "not set", areaTag = "not set";
FirebaseJson json1;
FirebaseJsonData jsonData;

//Stream Variables
//void printResult(FirebaseData &data);
void printResult(StreamData &data);

FirebaseData fbdo1;


void setup() {
  
  // Get unique Id
  byte mac[6];
  WiFi.macAddress(mac);
  uniq =  String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);

  swUSB.begin(115200, SWSERIAL_8N1, txPin, rxPin);
  
  
  
  
  Serial.begin(115200);
  pinMode(inPin, INPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(redLed, OUTPUT);

  digitalWrite(redLed, HIGH);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(ESPNAME);
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  //Setup Firebase

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(fbdo, 1000 * 60);
  Firebase.setwriteSizeLimit(fbdo, "tiny");
  Firebase.setFloatDigits(2);
  Firebase.setDoubleDigits(6);

  String newPath = "/ERCharts/devices/" + uniq;
  Serial.println( newPath);
  if (Firebase.getJSON(fbdo, newPath))
  {
    Serial.println("PASSED  QUERY");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
    Serial.println("ETag: " + fbdo.ETag());
    Serial.print("VALUE: ");
    printResult(fbdo);
    Serial.println("Location: " + location);
    Serial.println("Area:" + area);
    //Get Tag for Tickets

    path = path + "/" + location + "/areas/" + area + "/";

    if (Firebase.getString(fbdo, path + "queue/lastTicket"))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
      Serial.println("ETag: " + fbdo.ETag());
      Serial.print("VALUE: ");
      printResult(fbdo);
      toDay = fbdo.stringData().substring(0, 2).toInt();
      ptNumber = fbdo.stringData().substring(2, 4).toInt();
      Serial.println("------------------------------------");
      Serial.println();
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
    ///Add stream to devices config
    if (!Firebase.beginStream(fbdo1, newPath))
    {
      Serial.println("------------------------------------");
      Serial.println("Can't begin stream connection...");
      Serial.println("REASON: " + fbdo1.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
    Firebase.setStreamCallback(fbdo1, streamCallback, streamTimeoutCallback, 8192);
  }
  else {
    Serial.println( newPath);
    json1.set("deviceId", uniq);
    json1.set("name", ESPNAME);
    json1.set("location", 0);
    json1.set("area", 0);
    json1.set("devicetype", "ESP32");
    json1.set("notes", "Scangle printer");

    if (Firebase.setJSON(fbdo, newPath , json1))
    {
      Serial.println("Device not found, registering");
      Serial.println("PATH: " + fbdo.dataPath() + fbdo.pushName());
      Serial.println("TYPE: " + fbdo.dataType());
      Serial.print("VALUE: ");
      printResult(fbdo);
      Serial.println("------------------------------------");
      Serial.println();
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }

  //  tagsPath = "/ERCharts/Map/locations/" + location + "/name/" + area + "/";
  String tagsPath = "/ERCharts/Map/locations/" + location + "/name/";

  if (Firebase.getString(fbdo, tagsPath))
  {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
    Serial.println("ETag: " + fbdo.ETag());
    Serial.print("VALUE: ");
    printResult(fbdo);
    locationTag = fbdo.stringData();
    Serial.println("------------------------------------");
    Serial.println();
  }
  else
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }
  tagsPath = "/ERCharts/Map/locations/" + location + "/areas/" + area + "/name/";

  if (Firebase.getString(fbdo, tagsPath))
  {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
    Serial.println("ETag: " + fbdo.ETag());
    Serial.print("VALUE: ");
    printResult(fbdo);
    areaTag = fbdo.stringData();
    if (areaTag.length() > 30) {
      Serial.println("Area tag to large for one line");
      int sSpace = 0;
      for (int i = 0; i < areaTag.length(); i++) {
        sSpace = areaTag.indexOf(" ", sSpace + 1);
        Serial.print("Space in Index");
        Serial.print(i);
        Serial.print("sSpace Value");
        Serial.println(sSpace);
        if (sSpace > 30) {
          areaTag2 = areaTag.substring(sSpace);
          areaTag = areaTag.substring(0, sSpace);
          i = areaTag.length();
          Serial.print("Area Slpitted in two:");
          Serial.print("Area:" + areaTag);
          Serial.println("Area2:" + areaTag2);
        }

      }
    }
    Serial.println("------------------------------------");
    Serial.println();
  }
  else
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }

  //
  //  String lastNumber = Firebase.getString(path);
  //  toDay = lastNumber.substring(0, 2).toInt();
  //  ptNumber = lastNumber.substring(2, 4).toInt();
  //  Serial.print("\n ToDay<");
  //  Serial.print(toDay);
  //  Serial.print(">Pt Number:<");
  //  Serial.print(ptNumber);
  //  Serial.println(">");



  digitalWrite(redLed, LOW);


}


void checkBarcode(){
  while (swUSB.available()) {
    String message = swUSB.readStringUntil('\n');
      Serial.println(message);
  
  }
}

void loop() {
checkBarcode();
  
  ArduinoOTA.handle();
  //  Serial.println(touchRead(4));
  //  delay(600);


  val = digitalRead(inPin);
  if (val == HIGH) {
    if (buttonActive == false) {
      buttonActive = true;
      buttonTimer = millis();
    }
    if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {
      longPressActive = true;
    }
  } else {
    if (buttonActive == true) {
      digitalWrite(blueLed, HIGH);
      if (longPressActive == true) {
        longPressActive = false;
        printTimeTicket();
      } else {
        ptNumber++;
        printTicket();
        uploadFireBase();
        delay(500);
      }
      buttonActive = false;
    } else {
      digitalWrite(blueLed, LOW);
    }
  }
  if (now() > 62008) {
    if (toDay != day()) {
      toDay = day();
      toMonth = month();
      toYear = year();
      Serial.println("Restting Pt number to 0 from toDay !=day()");
      Serial.print("toDay: ");
      Serial.println(toDay);
      Serial.print("day() : ");
      Serial.println(day());
      ptNumber = 0;
    }
  }
  // time Server functions
  if (now() - ntpLastUpdate > ntpSyncTime || now() < 62008) {
    int tries = 0;
    while (!getTimeAndDate() && tries < 10) {
      tries++;
    }
    if (tries < 10) {
      Serial.print("ntp server update success. Server: ");
      Serial.println(timeServerIP);
      printTimeTicket();
      char todayF[11];
      sprintf(todayF, "%02d-%02d-%02d", year(), month(), day());
      if (toDay == 0) {
        toDay = day();
        toMonth = month();
        toYear = year();
        Serial.println("Restting Pt number to 0 from toDay == 0");
        Serial.print("today: ");
        Serial.println(toDay);
        Serial.print(" day(): ");
        Serial.println(day());
        //Edited but not sure
        //ptNumber = 0;
      }
    }
    else {
      Serial.println("ntp server update failed");
    }
  }
  if (now() - aliveLastUpdate > aliveSyncTime || now() < 62008) {
    setAlive();
  }
}


void printTimeTicket() {
 
}

void printTicket() {
  

}


void uploadFireBase() {
  int arrivalEpoch = now();
  char numberToDisplayChar[4];
  sprintf(numberToDisplayChar, "%02d%02d", day(), ptNumber);
  String numberToDisplay = numberToDisplayChar;
  Serial.print("New Patient<" + ptNumber);
  Serial.print(">Char<");
  Serial.print(numberToDisplayChar);
  Serial.println("> Number to Display<" + numberToDisplay);

  String base =  path + "tickets/" + arrivalEpoch + "/";
  //  base = base ;
  //  base = base + arrivalEpoch;
  //  base = base + "/ticketNumber/";
  //  json.clear().add("Data" + String(i + 1), i + 1);
  String arrivalTime = firebaseTimeDate();
  json1.clear();
  json1.set("deviceId", uniq);
  json1.set("ticketNumber", numberToDisplay);
  json1.set("ptStatus", "arrived");
  json1.set("arrivalDate", arrivalEpoch);
  json1.set("arrivalTime", arrivalTime);

  if (Firebase.setJSON(fbdo, base , json1))
  {
    //
    //  if (Firebase.setString(fbdo, base, String(numberToDisplay))) {
    Serial.println("FireBase Arduino Push on: " + base);
  } else {
    delay(1000);
    if (Firebase.setString(fbdo, base, String(numberToDisplay))) {
      delay(1000);
      Serial.println("FireBase Arduino Second Push on: " + base);
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }

  String baseTotal =  path + "queue/lastTicket/";
  //  baseTotal = baseTotal + location;
  //  baseTotal = baseTotal + "queue/lastTicket/";

  if (Firebase.setString(fbdo, baseTotal, String(numberToDisplay))) {
    Serial.println("FireBase Arduino Push Totals on:" + baseTotal);
    Serial.println("PATH: " + fbdo.dataPath() + fbdo.pushName());
    Serial.println("TYPE: " + fbdo.dataType());
    Serial.print("VALUE: ");
    printResult(fbdo);
    Serial.println("------------------------------------");
    Serial.println();

  } else {
    delay(1000);
    if (Firebase.setString(fbdo, baseTotal, String(numberToDisplay))) {
      Serial.println("FireBase Arduino second Push Totals on:" + baseTotal);
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }

}

void setAlive() {
  aliveLastUpdate = now();
  String alive = firebaseTimeDate();
  String alivePath =  path;
  alivePath = alivePath + "Stream/";
  if (location.equals("0")) {
    alivePath = basePath + "/";
  }
  alivePath = alivePath + uniq;

  Serial.print("Setting alive status<");
  Serial.print(alivePath);
  Serial.print(">Value:");
  Serial.println(alive);

  json1.clear();
  json1.set("deviceId", uniq);
  json1.set("name", ESPNAME);
  json1.set("location", location);
  json1.set("area", area);
  json1.set("wifi", wifiConn);
  json1.set("Date", alive);
  if (Firebase.setJSON(fbdo, alivePath , json1))
  {
    Serial.println("Device not found, registering");
    Serial.println("PATH: " + fbdo.dataPath() + fbdo.pushName());
    Serial.println("TYPE: " + fbdo.dataType());
    Serial.print("VALUE: ");
    printResult(fbdo);
    Serial.println("------------------------------------");
    Serial.println();
  }
  else
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }
}


void setAliveOld() {
  aliveLastUpdate = now();

  String alive = firebaseTimeDate();
  String alivePath =  path;
  alivePath = alivePath + "Stream/";

  if (location.equals("0")) {
    alivePath = basePath + "/";
  }


  alivePath = alivePath + uniq + "/" + ESPNAME;

  if (deadCount > 0) {
    alivePath = alivePath + "/dead/";
    deadCount = 0;
  } else {
    alivePath = alivePath + "/alive/";
  }
  alivePath = alivePath + wifiConn;
  alivePath = alivePath + "/";

  Serial.print("Setting alive status<");
  Serial.print(alivePath);
  Serial.print(">Value:");
  Serial.println(alive);


  if (Firebase.setString(fbdo, alivePath, alive)) {
    Serial.print("Alive set");
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
    // Add recurrence of function

  }


}

String firebaseTimeDate() {
  char timeToDisplay[20];
  sprintf(timeToDisplay, "%02d:%02d:%02d-%02d-%02d-%02d", hour(), minute(), second(), year(), month(), day());
  return timeToDisplay;
}
String timeToDisplayText() {
  char timeToDisplay[20];
  sprintf(timeToDisplay, "%02d:%02d:%02d %02d-%02d-%02d", hour(), minute(), second(), year(), month(), day());
  return timeToDisplay;
}

///Time server logic

int getTimeAndDate() {
  int flag = 0;
  Udp.begin(localPort);
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  delay(1000);
  if (Udp.parsePacket()) {
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord, lowWord, epoch;
    highWord = word(packetBuffer[40], packetBuffer[41]);
    lowWord = word(packetBuffer[42], packetBuffer[43]);
    epoch = highWord << 16 | lowWord;
    epoch = epoch - 2208988800 + timeZoneOffset;
    flag = 1;
    setTime(epoch);
    ntpLastUpdate = now();
  }
  return flag;
}

// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


/// firebase reference

void printResult(FirebaseData &data)
{

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean")
    Serial.println(data.boolData() == 1 ? "true" : "false");
  else if (data.dataType() == "string")
    Serial.println(data.stringData());
  else if (data.dataType() == "json")
  {
    Serial.println();
    FirebaseJson &json = data.jsonObject();
    //Print all object data
    Serial.println("Pretty printed JSON data:");
    String jsonStr;
    json.toString(jsonStr, true);
    Serial.println(jsonStr);
    Serial.println();
    Serial.println("Iterate JSON data:");
    Serial.println();
    size_t len = json.iteratorBegin();
    String key, value = "";
    int type = 0;
    for (size_t i = 0; i < len; i++)
    {
      json.iteratorGet(i, type, key, value);
      Serial.print(i);
      Serial.print(", ");
      Serial.print("Type: ");
      Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
      if (type == FirebaseJson::JSON_OBJECT)
      {
        Serial.print(", Key: ");
        Serial.print(key);
        if (key == "location") {
          Serial.println("Location Value set");
          location = value;
        }
        if (key == "area") {
          Serial.println("Area Value set");
          area = value;
        }
      }
      Serial.print(", Value: ");
      Serial.println(value);
    }
    json.iteratorEnd();
  }
  else if (data.dataType() == "array")
  {
    Serial.println();
    //get array data from FirebaseData using FirebaseJsonArray object
    FirebaseJsonArray &arr = data.jsonArray();
    //Print all array values
    Serial.println("Pretty printed Array:");
    String arrStr;
    arr.toString(arrStr, true);
    Serial.println(arrStr);
    Serial.println();
    Serial.println("Iterate array values:");
    Serial.println();
    for (size_t i = 0; i < arr.size(); i++)
    {
      Serial.print(i);
      Serial.print(", Value: ");

      FirebaseJsonData &jsonData = data.jsonData();
      //Get the result data from FirebaseJsonArray object
      arr.get(jsonData, i);
      if (jsonData.typeNum == FirebaseJson::JSON_BOOL)
        Serial.println(jsonData.boolValue ? "true" : "false");
      else if (jsonData.typeNum == FirebaseJson::JSON_INT)
        Serial.println(jsonData.intValue);
      else if (jsonData.typeNum == FirebaseJson::JSON_FLOAT)
        Serial.println(jsonData.floatValue);
      else if (jsonData.typeNum == FirebaseJson::JSON_DOUBLE)
        printf("%.9lf\n", jsonData.doubleValue);
      else if (jsonData.typeNum == FirebaseJson::JSON_STRING ||
               jsonData.typeNum == FirebaseJson::JSON_NULL ||
               jsonData.typeNum == FirebaseJson::JSON_OBJECT ||
               jsonData.typeNum == FirebaseJson::JSON_ARRAY)
        Serial.println(jsonData.stringValue);
    }
  }
  else if (data.dataType() == "blob")
  {

    Serial.println();

    for (size_t i = 0; i < data.blobData().size(); i++)
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      if (i < 16)
        Serial.print("0");

      Serial.print(data.blobData()[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  else if (data.dataType() == "file")
  {

    Serial.println();

    File file = data.fileStream();
    int i = 0;

    while (file.available())
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      int v = file.read();

      if (v < 16)
        Serial.print("0");

      Serial.print(v, HEX);
      Serial.print(" ");
      i++;
    }
    Serial.println();
    file.close();
  }
  else
  {
    Serial.println(data.payload());
  }
}

/// Stream Functions

void printResult(StreamData &data)
{

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean")
    Serial.println(data.boolData() == 1 ? "true" : "false");
  else if (data.dataType() == "string" || data.dataType() == "null")
    Serial.println(data.stringData());
  else if (data.dataType() == "json")
  {
    Serial.println();
    FirebaseJson *json = data.jsonObjectPtr();
    //Print all object data
    Serial.println("Pretty printed JSON data:");
    String jsonStr;
    json->toString(jsonStr, true);
    Serial.println(jsonStr);
    Serial.println();
    Serial.println("Iterate JSON data:");
    Serial.println();
    size_t len = json->iteratorBegin();
    String key, value = "";
    int type = 0;
    for (size_t i = 0; i < len; i++)
    {
      json->iteratorGet(i, type, key, value);
      Serial.print(i);
      Serial.print(", ");
      Serial.print("Type: ");
      Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
      if (type == FirebaseJson::JSON_OBJECT)
      {
        Serial.print(", Key: ");
        Serial.print(key);
      }
      Serial.print(", Value: ");
      Serial.println(value);
    }
    json->iteratorEnd();
  }
  else if (data.dataType() == "array")
  {
    Serial.println();
    //get array data from FirebaseData using FirebaseJsonArray object
    FirebaseJsonArray *arr = data.jsonArrayPtr();
    //Print all array values
    Serial.println("Pretty printed Array:");
    String arrStr;
    arr->toString(arrStr, true);
    Serial.println(arrStr);
    Serial.println();
    Serial.println("Iterate array values:");
    Serial.println();

    for (size_t i = 0; i < arr->size(); i++)
    {
      Serial.print(i);
      Serial.print(", Value: ");

      FirebaseJsonData *jsonData = data.jsonDataPtr();
      //Get the result data from FirebaseJsonArray object
      arr->get(*jsonData, i);
      if (jsonData->typeNum == FirebaseJson::JSON_BOOL)
        Serial.println(jsonData->boolValue ? "true" : "false");
      else if (jsonData->typeNum == FirebaseJson::JSON_INT)
        Serial.println(jsonData->intValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_FLOAT)
        Serial.println(jsonData->floatValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_DOUBLE)
        printf("%.9lf\n", jsonData->doubleValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_STRING ||
               jsonData->typeNum == FirebaseJson::JSON_NULL ||
               jsonData->typeNum == FirebaseJson::JSON_OBJECT ||
               jsonData->typeNum == FirebaseJson::JSON_ARRAY)
        Serial.println(jsonData->stringValue);
    }
  }
  else if (data.dataType() == "blob")
  {

    Serial.println();

    for (size_t i = 0; i < data.blobData().size(); i++)
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      if (i < 16)
        Serial.print("0");

      Serial.print(data.blobData()[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  else if (data.dataType() == "file")
  {

    Serial.println();

    File file = data.fileStream();
    int i = 0;

    while (file.available())
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      int v = file.read();

      if (v < 16)
        Serial.print("0");

      Serial.print(v, HEX);
      Serial.print(" ");
      i++;
    }
    Serial.println();
    file.close();
  }
}
void streamCallback(StreamData data)
{
  Serial.println("Stream Data1 available...");
  Serial.println("STREAM PATH: " + data.streamPath());
  Serial.println("EVENT PATH: " + data.dataPath());
  Serial.println("DATA TYPE: " + data.dataType());
  Serial.println("EVENT TYPE: " + data.eventType());
  Serial.print("VALUE: ");
  if (data.dataType() == "string")
  {
    Serial.print("Steam changed to:");
    Serial.println(data.stringData());
  }

  if (data.dataPath() == "/area") {
    Serial.print("Area Value:");
    printResult(data);
    if (data.stringData() != area) {
      Serial.println("Area changed from<" + area + "> to<" + data.stringData() + ">" );
      Serial.println("Reset to apply changes");
      ESP.restart();
    }
  }
  if (data.dataPath() == "/location") {
    Serial.print("Location changed to:");
    printResult(data);
    if (data.stringData() != location) {
      Serial.println("Location changed from<" + location + "> to<" + data.stringData() + ">" );
      Serial.println("Reset to apply changes");
      ESP.restart();
    }

  }
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
    Serial.println();
    Serial.println("Stream timeout, resume streaming...");
    Serial.println();
  }
}
