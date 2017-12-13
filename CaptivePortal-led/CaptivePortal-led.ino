/*
   Captive Portal by: M. Ray Burnette 20150831
   See Notes tab for original code references and compile requirements
 

   #####LOG
   
   -9 dec 2017 - Fixed ios captive portal crap =)))
   -for redirect specific urls check: http://www.esp8266.com/viewtopic.php?f=32&t=3618
   -base 64 imgs: https://www.hackster.io/rayburne/esp8266-captive-portal-5798ff
   -adding websockets led functionality: https://github.com/Links2004/arduinoWebSockets
   (depends on:  markus WebSockets lib)
   -13 dec 2017 - fixed arduino captive portal nagging
   
   
*/

#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include "./DNSServer.h"                  // Patched lib
#include <ESP8266WebServer.h>


static void writeLED(bool);
const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object
ESP8266WebServer  webServer(80);          // HTTP server
WebSocketsServer webSocket = WebSocketsServer(81);

//html page to be send to user:
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>ESP8266 WebSocket Demo</title>
<style>
"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
</style>
<script>
var websock;
function start() {
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) { console.log('websock open'); };
  websock.onclose = function(evt) { console.log('websock close'); };
  websock.onerror = function(evt) { console.log(evt); };
  websock.onmessage = function(evt) {
    console.log(evt);
    var e = document.getElementById('ledstatus');
    if (evt.data === 'ledon') {
      e.style.color = 'red';
    }
    else if (evt.data === 'ledoff') {
      e.style.color = 'black';
    }
    else {
      console.log('unknown event');
    }
  };
}
function buttonclick(e) {
  websock.send(e.id);
}
</script>
</head>
<body onload="javascript:start();">
<h1>ESP8266 WebSocket Demo</h1>
<div id="ledstatus"><b>LED</b></div>
<button id="ledon"  type="button" onclick="buttonclick(this);">On</button> 
<button id="ledoff" type="button" onclick="buttonclick(this);">Off</button>
</body>
</html>
)rawliteral";

// GPIO#0 is for Adafruit ESP8266 HUZZAH board. Your Wemos D1 Mini LED is built-in D4, Arduino is on 13.
const int LEDPIN = D4;
// Current LED status
bool LEDStatus;

// Commands sent through Web Socket
const char LEDON[] = "ledon";
const char LEDOFF[] = "ledoff";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // Send the current LED status
        if (LEDStatus) {
          webSocket.sendTXT(num, LEDON, strlen(LEDON));
        }
        else {
          webSocket.sendTXT(num, LEDOFF, strlen(LEDOFF));
        }
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\r\n", num, payload);

      if (strcmp(LEDON, (const char *)payload) == 0) {
        writeLED(true);
      }
      else if (strcmp(LEDOFF, (const char *)payload) == 0) {
        writeLED(false);
      }
      else {
        Serial.println("Unknown command");
      }
      // send data to all connected clients
      webSocket.broadcastTXT(payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}




String responseHTML = ""
                      "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
                      "<h1>Hello World!</h1><p>This is a captive portal example. All requests will "
                      "be redirected here.</p></body></html>";

//page found handling:
void handleRoot() { // send the html when user visits root url
  webServer.send_P(200, "text/html", INDEX_HTML);
}


void handleAndroid() { // fool android captive portal (android 5, 6, 7)
 // webServer.send(204);

    webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");
   webServer.send ( 204, "text/plain", "");
    
}



static void writeLED(bool LEDon)
{
  LEDStatus = LEDon;
  // Note inverted logic for Adafruit HUZZAH board
  if (LEDon) {
    digitalWrite(LEDPIN, 0);
  }
  else {
    digitalWrite(LEDPIN, 1);
  }
}


void setup() {
  pinMode(LEDPIN, OUTPUT);
  writeLED(false);

 Serial.begin(115200);
 
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("lol"); //ssid 

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError); //< not needed
  dnsServer.start(DNS_PORT, "*", apIP);



  webServer.on("/generate_204", handleAndroid);  //Android captive portal. Still requires to "sign in to network" ugghh, hack below fixes it
  //webServer.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  //wtf with friefox: http://detectportal.firefox.com/success.txt
  //webServer.on("/library", handleNotFound); //iOS captive portal.  Maybe not needed. Might be handled by notFound handler.
  webServer.on ( "/", handleRoot ); //handle 'normal' request, redirect all to handleRoot


  // replay to all requests except for requests to a web root (handled by handleRoot) with success HTML (to fool iOS captive portal) 
  webServer.onNotFound([]() {
     webServer.send ( 204, "text/plain", ""); // ugly hack, send two headers >> 'works' for android? webServer.send ( 204, "text/plain", "");
    webServer.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    
  });
  webServer.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  dnsServer.processNextRequest();
  webSocket.loop();
  webServer.handleClient();
}



