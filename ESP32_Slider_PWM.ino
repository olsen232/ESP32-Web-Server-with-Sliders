/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-slider-pwm/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

/* Modified from original version - now supports arbitrarily many sliders (numSliders). */

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Replace with your network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

const char* INDEX_PARAM = "index";
const char* VALUE_PARAM = "value";

// Data that we store per-slider (on the Arduino, not in the webpage).
struct Slider {
  int output;
  int ledChannel;
  int freq;
  int resolution;
  int value;
};
typedef struct Slider Slider;

// And we have this many sliders:
const int numSliders = 2;
Slider sliders[numSliders];

// These sliders are configured in init()

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Web Server</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.3rem;}
    p {font-size: 1.9rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #FFD65C;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; }
  </style>
  <script>
INIT_SLIDER_DATA = %INIT_SLIDER_DATA%;

function initSliders() {
  // Find slider-0, which is always present initially. We'll use it as a template.
  var slider_0 = document.getElementById("slider-0");
  var parentNode = slider_0.parentNode;
  // Remove it from the document - we'll start over.
  parentNode.removeChild(slider_0);

  // For each entry in INIT_SLIDER_DATA, add a fresh copy of slider-0.
  for (var i in INIT_SLIDER_DATA) {
    var slider_i = slider_0.cloneNode(/*deep=*/true);
    slider_i.id = "slider-" + i;
    var value = INIT_SLIDER_DATA[i];
    // Update the label and the slider itself to the value from INIT_SLIDER_DATA.
    slider_i.children[0].children[0].innerHTML = value;
    slider_i.children[1].children[0].value = value;
    parentNode.appendChild(slider_i);
  }
}

function sliderChanged(inputElement) {
  // inputElement is the <input> element that called this function.
  var sliderValue = inputElement.value;                         // The number from 0 to 255
  var divElement = inputElement.parentNode.parentNode;          // The <div> element containing the <input> element.
  var sliderId = divElement.id;                                 // Eg "slider-0", "slider-1", etc.
  var labelElement = divElement.children[0].children[0];        // The <span> element above the slider.
  labelElement.innerHTML = sliderValue;                         // Update the text in the <span> element.

  // This is logged to the console. You can see this log (and any error messages) by opening the developer console:
  // Right click the webpage -> click "inspect" -> click the "console" tab in the developer tools.
  console.log(sliderId + " -> " + sliderValue);

  // Pull the number part out of sliderId:
  var sliderIndex = parseInt(sliderId.match(/\d+/)[0]);  // Eg 0, 1, etc.

  // The request looks something like this: GET /slider?index=0&value=255
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?index=" + sliderIndex + "&value="+sliderValue, true);
  xhr.send();
}
  </script>
</head>
<body onload="initSliders()">
  <h2>ESP Web Server</h2>
  <div id="slider-0">
    <p><span>0</span></p>
    <p><input type="range" onchange="sliderChanged(this)" min="0" max="255" value="0" step="1" class="slider"></p>
  </div>
</body>
</html>
)rawliteral";

// Replaces INIT_SLIDER_DATA placeholder with a javascript array.
String processor(const String& var){
  //Serial.println(var);
  if (var == "INIT_SLIDER_DATA"){
    String result = String("[");
    result.reserve(6 * numSliders);
    for (int i = 0; i < numSliders; i++) {
      result += sliders[i].value;
      result += ", ";
    }
    result.remove(result.lastIndexOf(','));
    result += "]";
    return result;
  }
  return String();
}

void setup(){
  // Slider 0 (we now use zero-indexing everywhere):
  sliders[0].output = 4;
  sliders[0].ledChannel = 0;
  sliders[0].freq = 5000;
  sliders[0].resolution = 8;
  sliders[0].value = 0;

  // Slider 1:
  sliders[1].output = 16;
  sliders[1].ledChannel = 1;
  sliders[1].freq = 5000;
  sliders[1].resolution = 8;
  sliders[1].value = 0;

  // Serial port for debugging purposes
  Serial.begin(115200);

  for (int i = 0; i < numSliders; i++) {
    // configure LED PWM functionalitites
    ledcSetup(sliders[i].ledChannel, sliders[i].freq, sliders[i].resolution);

    // attach the channel to the GPIO to be controlled
    ledcAttachPin(sliders[i].output, sliders[i].ledChannel);

    // Write the initial value to the channel:
    ledcWrite(sliders[i].ledChannel, sliders[i].value);
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/slider?index=<zero-based-index>&value=<value>

  server.on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(INDEX_PARAM) && request->hasParam(VALUE_PARAM)) {
      int index = request->getParam(INDEX_PARAM)->value().toInt();
      int value = request->getParam(VALUE_PARAM)->value().toInt();

      Serial.print("GET /slider index=");
      Serial.print(index);
      Serial.print(" value=");
      Serial.println(value);

      // Update the stored slider value, and ledcWrite it.
      sliders[index].value = value;
      ledcWrite(sliders[index].ledChannel, sliders[index].value);
    } else {
      Serial.println("GET /slider - Message not valid");
    }
    request->send(200, "text/plain", "OK");
  });

  // Start server
  server.begin();
}

void loop() {

}
