/*
  TO DO: Sketch Drescription
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <rBase64.h>
#include <ArduinoJson.h>
//#include <StreamUtils.h>

#include "joomla.h"
#include "arduino_secrets.h"

int totalJoomlaUsers;

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// Wifi Settings ///////
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const String host = String(SITE);
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = FINGERPRINT; //j4.jugmi.it

void setup() {
  
  Serial.begin(9600);
  delay(5000);
  printJoomlaHead();

  Serial.println("==============================");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("[Attempting to connect to Network named: ");
  Serial.print(ssid);                   // print the network name (SSID);
  Serial.println("]");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
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
  
}

void loop() {

  String url = "/api/index.php/v1/users";

  totalJoomlaUsers = 0;

  // Initialize helpful response information
  String linksSelf;
  String linksPrevious;
  String linksNext;
  String linksLast;
  long   totalPages;
  String baseUrl = url;

  String authorization;
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
  
  do {

    int usersInPage = 0;

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("[Connecting to: ");
    Serial.print(host);
    Serial.println("]");
    Serial.printf("[Using fingerprint: %s]\n", fingerprint);
    client.setFingerprint(fingerprint);
  
    if (!client.connect(host, httpsPort)) {
      Serial.println(">> Connection failed");
      client.stop();
      return;
    } else {
      Serial.println(">> Connected");

      Serial.print("[Sending a GET request to URL: ");
      Serial.print(host);
      Serial.print(url);
      Serial.println("]");

      client.println("GET " + url + " HTTP/1.1");
      client.println("Host: " + host);
      client.println("Cache-Control: no-cache");
      client.println("Accept: */*");
      client.println("User-Agent: JoomlaApiCrawler-ESP8266");
      client.println("Content-Type: application/json");
      client.println("Authorization: " + authorization);
      client.println("Connection: close");
      client.println("");
    
      Serial.println(">> Request sent");
    
      Serial.println("[Response:]");
  
      // Allocate the JSON document
      // Use arduinojson.org/v6/assistant to compute the capacity.
      const size_t capacity = JSON_ARRAY_SIZE(20) + JSON_OBJECT_SIZE(1) + 22*JSON_OBJECT_SIZE(3) + 20*JSON_OBJECT_SIZE(6) + 2850;
      DynamicJsonDocument doc(capacity);
  
      while (client.connected() || client.available()) {
  
        if(client.available()) {
  
          // Parse Headers
          Serial.println("[Headers:]");
    
          while (true) {
            String headerLine = client.readStringUntil('\n');
            if (headerLine == "\r") { // Read end header line
              break;
            }
            Serial.println(headerLine);
          }
    
          // Parse Body
          Serial.print("[Body ");
          String hexBodyLength = client.readStringUntil('\n');
          hexBodyLength.trim();
          Serial.print("(length " + String(hexToDec(hexBodyLength)) + " char)");
          Serial.println(":]");

          yield();
          client.setTimeout(2000);
          String bodyLine = client.readStringUntil('\n');
          
          String lastLine = client.readStringUntil('\n');  // Read the last line with a 0 who rappresent ????
            Serial.print(">> Last Line: ");
            Serial.println(lastLine);
          client.readStringUntil('\n'); // Read end body line
  
          // Deserialize the JSON document
          DeserializationError err = deserializeJson(doc, bodyLine);
          //ReadLoggingStream loggingStream(bodyLine, Serial);
          //DeserializationError err = deserializeJson(doc, loggingStream);
    
          // Test if parsing succeeds.
          if (err) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(err.c_str());
            return;
          }

          // Print the body response
          serializeJsonPretty(doc, Serial);  //<----- ERROR

          
          /*
          JsonArray arr = doc.as<JsonArray>();
          for (JsonVariant value : arr) {
            Serial.println(value.as<char*>());
          }
          */
          /*String bodyResponse = doc.as<String>();
          int idx = 0;
          while(bodyResponse[idx] != '\0')
          {
           Serial.print(bodyResponse[idx]);
           idx++;
          }
          Serial.println();
          */
          //Serial.println("body response - body response - body response - body response");
    
          //Extract users info
          linksSelf     = doc["links"]["self"].as<String>();
          linksPrevious = doc["links"]["previous"].as<String>();
          linksNext     = doc["links"]["next"].as<String>();
          linksLast     = doc["links"]["last"].as<String>();
          totalPages    = doc["meta"]["total-pages"];
          usersInPage   = doc["data"].as<JsonArray>().size();
          Serial.println("linksSelf: "      + linksSelf);
          Serial.println("linksPrevious: "  + linksPrevious);
          Serial.println("linksNext: "      + linksNext);
          Serial.println("linksLast: "      + linksLast);
          Serial.println("totalPages: "     + String(totalPages));
          Serial.println("usersInPage: "    + String(usersInPage));

          url = linksNext.substring(linksNext.indexOf(baseUrl));
          
        }
  
      }

      // Disconnect
      client.stop();
      Serial.println("[Disconnected]");
      Serial.println();

    }

    totalJoomlaUsers = totalJoomlaUsers * (totalPages -1) + usersInPage;

    delay(100);

  } while( !(totalPages == 1 && linksPrevious == "null" && linksNext == "null" && linksLast == "null") // Only one page
        && !(totalPages != 1 && linksPrevious != "null" && linksNext == "null" && linksLast == "null") // This is the last page
         );

  Serial.println("Total Joomla users for " + String(host) + ": " + String(totalJoomlaUsers));
  Serial.println();

  // Wait
  int minutes = 1;
  Serial.println(">> Wait " + String(minutes) + " minutes");
  Serial.println("==============================");
  delay(minutes*60*1000);
}


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
