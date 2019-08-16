#define FASTLED_ALLOW_INTERRUPTS 0

#include <Arduino.h>
#include <ESP8266WiFiMulti.h>

#include <MQTT.h>
#include "FastLED.h"

ESP8266WiFiMulti net;
WiFiClient wifiClient;
MQTTClient client;

CRGB leds[12];

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
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

// This function modifies 'cur' in place.
CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
{
  uint8_t r = cur.red;
  uint8_t g = cur.green;
  uint8_t b = cur.blue;

	nblendU8TowardU8( r, target.red,   amount);
  nblendU8TowardU8( g, target.green, amount);
  nblendU8TowardU8( b, target.blue,  amount);

  return CRGB(r,g,b);
}

void setupParts() {
	client.publish("homie/ledring/$homie", "4.0.0", true, 1);
	client.publish("homie/ledring/$name", "LED Ring", true, 1);
	client.publish("homie/ledring/$nodes", "ring", true, 1);
	client.publish("homie/ledring/$extensions", "", true, 1);
	client.publish("homie/ledring/ring/$name", "Ring", true, 1);
	client.publish("homie/ledring/ring/$type", "Neopixel", true, 1);
	client.publish("homie/ledring/ring/$properties", "color", true, 1);
	client.publish("homie/ledring/ring/color/$name", "Farbe", true, 1);
	client.publish("homie/ledring/ring/color/$settable", "true", true, 1);
	client.publish("homie/ledring/ring/color/$datatype", "color", true, 1);
	client.publish("homie/ledring/ring/color/$format", "rgb", true, 1);
	client.publish("homie/ledring/ring/color", "0,0,0", true, 1);
}

void connect() {
  Serial.print("checking wifi...");

	while (net.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  client.setWill("homie/ledring/$state", "lost", true, 1);

  Serial.print("\nconnecting...");

  while (!client.connect("color-ring")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  setupParts();

	client.subscribe("homie/ledring/ring/color/set");
  client.publish("homie/ledring/$state", "ready", true, 1);
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

		for(int i=0; i < 12; i++) {
    	leds[i] = currentColor;
  	}

		FastLED.show();
		delay(3);
	}

	client.publish("homie/ledring/ring/color", payload, true, 1);
}

void setup() {
	Serial.begin(115200);
	delay(5000);

	FastLED.addLeds<NEOPIXEL, D2>(leds, 12);

	for(int i=0; i < 12; i++) {
    leds[i] = CRGB::Black;
  }

	FastLED.show();

	net.addAP("***REMOVED***", "***REMOVED***");

	while (net.run() != WL_CONNECTED) {
		Serial.print(".");
	    delay(250);
	}

	client.begin("192.168.178.206", wifiClient);
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
