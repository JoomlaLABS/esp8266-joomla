/*
  TO DO: Sketch Drescription
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <rBase64.h>
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <time.h>

#define DHTinternalPin 12    //D6 of NodeMCU is GPIO14
#define DHTexternalPin 14    //D5 of NodeMCU is GPIO14

DHTesp DHTinternal;
DHTesp DHTexternal;

const long timezone = +2;

String wday_name[7] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};
String mon_name[12]= {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};

#include "joomla.h"
#include "arduino_secrets.h"
String authorization;

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// Wifi Settings ///////
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const String host = String(SITE);
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = FINGERPRINT;

void setup() {

  // Initialize Serial port
  Serial.begin(115200);
  while (!Serial) continue;
  delay(5000);
  printJoomlaHead();

  Serial.println("==============================");

  // Initialize WIFI library
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.printf("[Attempting to connect to Network named: %s]\n", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.println(">> WiFi connected");
  // print the SSID of the network you're attached to:
  Serial.print(">> SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  Serial.print(">> IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("==============================");

  // Initialize Joomla! authorization
  if (JOOMLA_TOKEN!=NULL) {
    authorization = "Bearer " + String(JOOMLA_TOKEN);
  } else if (JOOMLA_PASS!=NULL) {
    rbase64.encode(String(JOOMLA_USER) + ":" + String(JOOMLA_PASS));
    authorization = "Basic " + String(rbase64.result());
  } else  {
    Serial.println("Joomla! API Authentication not set in the Secret tab/arduino_secrets.h");
    // Exit the loop()
    exit(0);  
  }
  
}

void loop() {
  
  String url = "/api/index.php/v1/content/article";

  // Get current time from NTP
  time_t now = setClock();
  struct tm* timeInfo;
  timeInfo = localtime(&now);  
  
  String TodayString = wday_name[timeInfo->tm_wday] + " "
                      + String(timeInfo->tm_mday) + " "
                      + mon_name[timeInfo->tm_mon] + " " 
                      + String(1900 + timeInfo->tm_year);
  String TodayDate = String(1900 + timeInfo->tm_year) + "-"
                      + String(timeInfo->tm_mon < 10 ? "0" + String(timeInfo->tm_mon) : timeInfo->tm_mon) + "-" 
                      + String(timeInfo->tm_mday < 10 ? "0" + String(timeInfo->tm_mday) : timeInfo->tm_mday);
  String NowString = String(timeInfo->tm_hour < 10 ? "0" + String(timeInfo->tm_hour) : timeInfo->tm_hour) + ":"
                      + String(timeInfo->tm_min < 10 ? "0" + String(timeInfo->tm_min) : timeInfo->tm_min) + ":"
                      + String(timeInfo->tm_sec < 10 ? "0" + String(timeInfo->tm_sec) : timeInfo->tm_sec);
  
  float internalHumidity, internalTemperature;
  float externalHumidity, externalTemperature;
  
  DHTinternal.setup(DHTinternalPin, DHTesp::DHT11); //for DHT11 Connect DHT sensor to DHTinternalPin
  DHTexternal.setup(DHTexternalPin, DHTesp::DHT11); //for DHT11 Connect DHT sensor to DHTexternalPin
  //dht.setup(DHTpin, DHTesp::DHT22); //for DHT22 Connect DHT sensor to DHTpin
  
  //Get Humidity temperatue data after request is complete
  //Give enough time to handle client to avoid problems
  delay(DHTinternal.getMinimumSamplingPeriod());
  
  internalHumidity = DHTinternal.getHumidity();
  internalTemperature = DHTinternal.getTemperature();
  
  delay(DHTexternal.getMinimumSamplingPeriod());
  
  externalHumidity = DHTexternal.getHumidity();
  externalTemperature = DHTexternal.getTemperature();

  Serial.println("==============================");
  Serial.print(asctime(timeInfo));
  Serial.println("            |   INTERNAL   |   EXTERNAL");
  Serial.println("------------------------------------------");
  Serial.println("Temperature |   "+ String(internalTemperature) + "°C    |   " + String(externalTemperature) +"°C");
  Serial.println("Humidity    |   "+ String(internalHumidity) + "%     |   " + String(externalHumidity) +"%");
  Serial.println("==============================");

  // Connect to HTTPS server
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  yield();
  client.setTimeout(2000);
  Serial.println("[Connecting to: " + host + "]");
  Serial.printf("[Using fingerprint: %s]\n", fingerprint);
  client.setFingerprint(fingerprint);

  if (!client.connect(host, httpsPort)) {
    Serial.println(">> Connection failed");
    client.stop();
    return;
  }
  
  Serial.println(">> Connected");

  // Send HTTPS request
  Serial.print("[Sending a GET request to URL: ");
  Serial.print(host);
  String articleTitle = "Meteo " + TodayString;
  String articleTitleFilter = articleTitle;
  articleTitleFilter.replace(" ", "%20");
  Serial.print(url + "?filter[search]=" + articleTitleFilter);
  Serial.println("]");
  
  client.println("GET " + url + "?filter[search]=" + articleTitleFilter + " HTTP/1.1");
  client.println("Host: " + host);
  client.println("Cache-Control: no-cache");
  client.println("Accept: */*");
  client.println("User-Agent: JoomlaApiCrawler-ESP8266");
  client.println("Content-Type: application/json");
  client.println("Authorization: " + authorization);
  client.println("Connection: close");
  if (client.println("") == 0) {
    Serial.println(">> Failed to send request");
    return;
  }

  Serial.println(">> Request sent");

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(">> Unexpected response: ");
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(">> Invalid response");
    return;
  }

  Serial.println(">> Response OK and Valid response");

  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(1) + 4*JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(10) + 10000;
  DynamicJsonDocument doc(capacity);


  // Parse Body
  Serial.print("[Body ");
  String hexBodyLength = client.readStringUntil('\n');
  hexBodyLength.trim();
  Serial.print("(length " + String(hexToDec(hexBodyLength)) + " char)");
  Serial.println(":]");

  // Parse Body Content
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client.readStringUntil('\n'));
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  // Print the Pretty Content
  serializeJsonPretty(doc, Serial);
  Serial.println();

  // Parse Body last line
  String lastLine = client.readStringUntil('\n');  // Read the last line with a 0 who rappresent ????
  //Serial.print(">> Last Line: ");
  //Serial.println(lastLine);
  client.readStringUntil('\n'); // Read end body line

  // Insert new weatherdetection
  String detection = "\"field2\":\"" + String(externalTemperature) + "\"," +
                     "\"field1\":\"" + String(internalTemperature) + "\"," +
                     "\"field4\":\"" + String(externalHumidity) + "\"," +
                     "\"field3\":\"" + String(internalHumidity) + "\"," +
                     "\"field5\":\"" + TodayDate + " " + NowString + "\"";

  String method;
  String data;
  
  if(doc["meta"]["total-pages"] == 0) {
    // Create new article
    Serial.println("[Create new article]");

    method = "POST";
    int articleCatid = 8;

    //String weatherdetections = String("{\"row0\":{") + detection + "}}";

    data = String("{") +
                  "\"title\" : \"" + articleTitle + "\"," +
                  "\"catid\" : \"" + String(articleCatid) + "\"," +
                  "\"articletext\": \"<p>Meteo della baita di " + TodayString + "</p>\"," +
                  "\"state\" : 1," +
                  "\"transition\" : 2," +
                  "\"language\": \"*\"," +
                  "\"metadesc\": \"\"," +
                  "\"metakey\": \"\"," +
                  //"\"weatherdetection\" : " + weatherdetections +
                  "\"weatherdetection\" : " + "{\"row0\":{" + detection + "}}" +
                  "}";
    
  } else {
    // Update existing article
    Serial.println("[Update existing article]");

    method = "PATCH";
    int article_id = doc["data"][0]["attributes"]["id"];
    url = url + "/" + String(article_id);
    int articleCatid = doc["data"][0]["relationships"]["category"]["data"]["id"];
    
    //String weatherdetections = doc["data"][0]["attributes"]["weatherdetection"];
    //int rowId = weatherdetections.substring(weatherdetections.lastIndexOf("row")+3, weatherdetections.lastIndexOf(":{")-1).toInt() + 1;
    //String newdetection = "\"row" + String(rowId) + "\":{" + detection + "}";
    //weatherdetections.replace("}}", "}," + newdetection + "}");

    String weatherdetections = doc["data"][0]["attributes"]["weatherdetection"];
    int rowId = weatherdetections.substring(weatherdetections.lastIndexOf("row")+3, weatherdetections.lastIndexOf(":{")-1).toInt() + 1;
    weatherdetections.replace("}}", String("},") + "\"row" + String(rowId) + "\":{" + detection + "}" + "}");

    data = String("{") +
                  "\"title\" : \"" + articleTitle + "\"," +
                  "\"catid\" : \"" + String(articleCatid) + "\"," +
                  //"\"weatherdetection\" : " + weatherdetections +
                  "\"weatherdetection\" : " + weatherdetections +
                  "}";

  }

  // Connect to HTTPS server
  // Use WiFiClientSecure class to create TLS connection
  client = WiFiClientSecure();
  yield();
  client.setTimeout(2000);
  Serial.println("[Connecting to: " + host + "]");
  Serial.printf("[Using fingerprint: %s]\n", fingerprint);
  client.setFingerprint(fingerprint);

  if (!client.connect(host, httpsPort)) {
    Serial.println(">> Connection failed");
    client.stop();
    return;
  }
  
  Serial.println(">> Connected");
  
  // Send HTTPS request
  Serial.print("[Sending a " + method + " request to URL: ");
  Serial.print(host);
  Serial.print(url);
  Serial.println("]");

  Serial.println(data);
  //client.println("PATCH /lucat_test.php HTTP/1.1");
  //client.println("Host: j4.jugmi.it");
  client.println(method + " " + url + " HTTP/1.1");
  client.println("Host: " + host);
  client.println("Cache-Control: no-cache");
  client.println("Accept: */*");
  client.println("User-Agent: JoomlaApiCrawler-ESP8266");
  client.println("Content-Type: application/json");
  client.println("Authorization: " + authorization);
  client.println("Connection: close");
  client.println("Content-Length: " + String(data.length()));
  client.println("");
  client.println(data);
  if (client.println("") == 0) {
    Serial.println(">> Failed to send request");
    return;
  }

  Serial.println(">> Request sent");
  
  delay(100);

  // Check HTTP status
  status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(">> Unexpected response: ");
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  if (!client.find(endOfHeaders)) {
    Serial.println(">> Invalid response");
    return;
  }

  Serial.println(">> Response OK and Valid response");



  // Disconnect
  client.stop();
  Serial.println("[Disconnected]");
  Serial.println("==============================");

  // Wait
  int minutes = 60;
  Serial.println("[Waiting " + String(minutes) + " minutes]");
  for(int i = 0; i <= minutes * 60; i++) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.println("==============================");
  
}


/*
  Helper functions
*/

// Convert hex value to int
unsigned int hexToDec(String hexString) {
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}

// Set time via NTP
time_t setClock() {
  configTime(-1 * timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.println("[Waiting for NTP time sync]");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(1000);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  return now;
}
