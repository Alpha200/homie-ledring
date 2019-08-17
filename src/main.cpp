#define FASTLED_ALLOW_INTERRUPTS 0

#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <MQTT.h>
#include "FastLED.h"

#include "secrets.h"

#define NUM_LEDS 12
#define LED_PIN D2
#define DEVICE_ID "ledring"

ESP8266WiFiMulti net;
WiFiClient wifiClient;
MQTTClient client;

CRGB leds[NUM_LEDS];

void nblendU8TowardU8(uint8_t& cur, const uint8_t target, uint8_t amount) {
  if( cur == target) return;

  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
}

CRGB fadeTowardColor(const CRGB& cur, const CRGB& target, uint8_t amount) {
  uint8_t r = cur.red;
  uint8_t g = cur.green;
  uint8_t b = cur.blue;

	nblendU8TowardU8(r, target.red, amount);
  nblendU8TowardU8(g, target.green, amount);
  nblendU8TowardU8(b, target.blue, amount);

  return CRGB(r,g,b);
}

void setupParts() {
	client.publish("homie/" DEVICE_ID "/$homie", "4.0.0", true, 1);
	client.publish("homie/" DEVICE_ID "/$name", "LED Ring", true, 1);
	client.publish("homie/" DEVICE_ID "/$nodes", "ring", true, 1);
	client.publish("homie/" DEVICE_ID "/$extensions", "", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/$name", "Ring", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/$type", "Neopixel", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/$properties", "color", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/color/$name", "Color", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/color/$settable", "true", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/color/$datatype", "color", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/color/$format", "rgb", true, 1);
	client.publish("homie/" DEVICE_ID "/ring/color", "0,0,0", true, 1);
}

void connect() {
  Serial.print("checking wifi...");

	while (net.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  client.setWill("homie/" DEVICE_ID "/$state", "lost", true, 1);

  Serial.print("\nconnecting...");

  while (!client.connect("color-ring")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  setupParts();

	client.subscribe("homie/" DEVICE_ID "/ring/color/set");
  client.publish("homie/" DEVICE_ID "/$state", "ready", true, 1);
}

void showColor(const CRGB& color) {
  for(int i=0; i < NUM_LEDS; i++) {
    	leds[i] = color;
  }

	FastLED.show();
}

void messageReceived(String &topic, String &payload) {
	int r, g, b;

	int first = payload.indexOf(',');
	int last = payload.lastIndexOf(',');

	r = payload.substring(0, first).toInt();
	g = payload.substring(first + 1, last).toInt();
	b = payload.substring(last+1).toInt();

	CRGB color(r, g, b);
	CRGB prevColor = leds[0];

	for (uint8_t j=0; j < 255; j++) {
		CRGB currentColor = fadeTowardColor(prevColor, color, j);
    showColor(currentColor);
		delay(3);
	}

  showColor(color);
	client.publish("homie/" DEVICE_ID "/ring/color", payload, true, 1);
}

void setup() {
	Serial.begin(115200);
	delay(5000);

	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  showColor(CRGB::Black);

	net.addAP(WIFI_SSID, WIFI_PASSWORD);

	while (net.run() != WL_CONNECTED) {
		Serial.print(".");
	    delay(250);
	}

	client.begin(MQTT_SERVER, wifiClient);
	client.onMessage(messageReceived);

	connect();
}

void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }
}
