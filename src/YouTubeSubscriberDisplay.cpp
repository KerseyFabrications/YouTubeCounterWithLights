/*******************************************************************
 *  Read YouTube Channel statistics from the YouTube API           *
 *  This sketch uses the WiFiManager Library for configuraiton     *
 *  Using DoubleResetDetector to launch config mode                *
 *                                                                 *
 *  By Brian Lough                                                 *
 *  https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA       *
 *                                                                 *
 *  By Kris Kersey                                                 *
 *  https://www.youtube.com/KerseyFabrications                     *
 *  Modifications to add new display and NeoPixel LED for          *
 *  play button.                                                   *
 *******************************************************************/

#include <Arduino.h>

#include <YoutubeApi.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <DoubleResetDetector.h>
// For entering Config mode by pressing reset twice
// Available on the library manager (DoubleResetDetector)
// https://github.com/datacute/DoubleResetDetector

#include <ArduinoJson.h>
// Required for the YouTubeApi and used for the config file
// Available on the library manager (ArduinoJson)
// https://github.com/bblanchon/ArduinoJson

#include <WiFiManager.h>
// For configuring the Wifi credentials without re-programing
// Availalbe on library manager (WiFiManager)
// https://github.com/tzapu/WiFiManager

// For storing configurations
#include "FS.h"

// Additional libraries needed by WiFiManager
#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include <LEDMatrixDriver.hpp>
// For writing to the LED Display
// Available through library manager (LEDMatrixDriver)
// https://github.com/bartoszbielawski/LEDMatrixDriver

#include <Adafruit_NeoPixel.h>
// For lighting up NeoPixel(s) for play button.
// Available through library manager (Adafruit NeoPixel)
// https://github.com/adafruit/Adafruit_NeoPixel

// NeoPixel Defines
#define NP_PIN 4
#define NUMPIXELS 1

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NP_PIN, NEO_RGB);

char apiKey[45] = "";
char channelId[30] = "";

WiFiClientSecure client;
YoutubeApi *api, *newapi;

unsigned long api_mtbs = 60000; //mean time between api requests
unsigned long api_lasttime;   //last time api request has been done

long subs = 0;

// flag for saving data
bool shouldSaveConfig = false;

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
// This sketch uses drd.stop() rather than relying on the timeout
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// Define the ChipSelect pin for the led matrix (Dont use the SS or MISO pin of your Arduino!)
// Other pins are arduino specific SPI pins (MOSI=DIN of the LEDMatrix and CLK) see https://www.arduino.cc/en/Reference/SPI
const uint8_t LEDMATRIX_CS_PIN = 15;

// Define LED Matrix dimensions (0-n) - eg: 32x8 = 31x7
const int LEDMATRIX_WIDTH = 31;
const int LEDMATRIX_HEIGHT = 7;
const int LEDMATRIX_SEGMENTS = 4;

// The LEDMatrixDriver class instance
LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);

int x=0, y=0;   // start top left

// This is the font definition. You can use http://gurgleapps.com/tools/matrix to create your own font or sprites.
// If you like the font feel free to use it. I created it myself and donate it to the public domain.
// Kris Kersey: I have modified all of the numbers and the "," to take less space.
byte font[95][8] = { {0,0,0,0,0,0,0,0}, // SPACE
                     {0x10,0x18,0x18,0x18,0x18,0x00,0x18,0x18}, // EXCL
                     {0x28,0x28,0x08,0x00,0x00,0x00,0x00,0x00}, // QUOT
                     {0x00,0x0a,0x7f,0x14,0x28,0xfe,0x50,0x00}, // #
                     {0x10,0x38,0x54,0x70,0x1c,0x54,0x38,0x10}, // $
                     {0x00,0x60,0x66,0x08,0x10,0x66,0x06,0x00}, // %
                     {0,0,0,0,0,0,0,0}, // &
                     {0x00,0x10,0x18,0x18,0x08,0x00,0x00,0x00}, // '
                     {0x02,0x04,0x08,0x08,0x08,0x08,0x08,0x04}, // (
                     {0x40,0x20,0x10,0x10,0x10,0x10,0x10,0x20}, // )
                     {0x00,0x10,0x54,0x38,0x10,0x38,0x54,0x10}, // *
                     {0x00,0x08,0x08,0x08,0x7f,0x08,0x08,0x08}, // +
                     {0x00,0x00,0x00,0x00,0x00,0xc0,0x40,0x80}, // COMMA
                     {0x00,0x00,0x00,0x00,0x7e,0x00,0x00,0x00}, // -
                     {0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x06}, // DOT
                     {0x00,0x04,0x04,0x08,0x10,0x20,0x40,0x40}, // /
                     {0x60,0x90,0x90,0x90,0x90,0x90,0x60,0x00}, // 0
                     {0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00}, // 1
                     {0x60,0x90,0x10,0x20,0x40,0x80,0xf0,0x00}, // 2
                     {0x60,0x90,0x10,0x60,0x10,0x90,0x60,0x00}, // 3
                     {0x80,0xa0,0xa0,0xf0,0x20,0x20,0x20,0x00}, // 4
                     {0xf0,0x80,0xe0,0x10,0x10,0x90,0x60,0x00}, // 5
                     {0x60,0x90,0x80,0xe0,0x90,0x90,0x60,0x00}, // 6
                     {0xf0,0x10,0x20,0x40,0x40,0x40,0x40,0x00}, // 7
                     {0x60,0x90,0x90,0x60,0x90,0x90,0x60,0x00}, // 8
                     {0x60,0x90,0x90,0x70,0x10,0x90,0x60,0x00}, // 9
                     {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
                     {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x08}, // ;
                     {0x00,0x10,0x20,0x40,0x80,0x40,0x20,0x10}, // <
                     {0x00,0x00,0x7e,0x00,0x00,0xfc,0x00,0x00}, // =
                     {0x00,0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // >
                     {0x00,0x38,0x44,0x04,0x08,0x10,0x00,0x10}, // ?
                     {0x00,0x30,0x48,0xba,0xba,0x84,0x78,0x00}, // @
                     {0x00,0x1c,0x22,0x42,0x42,0x7e,0x42,0x42}, // A
                     {0x00,0x78,0x44,0x44,0x78,0x44,0x44,0x7c}, // B
                     {0x00,0x3c,0x44,0x40,0x40,0x40,0x44,0x7c}, // C
                     {0x00,0x7c,0x42,0x42,0x42,0x42,0x44,0x78}, // D
                     {0x00,0x78,0x40,0x40,0x70,0x40,0x40,0x7c}, // E
                     {0x00,0x7c,0x40,0x40,0x78,0x40,0x40,0x40}, // F
                     {0x00,0x3c,0x40,0x40,0x5c,0x44,0x44,0x78}, // G
                     {0x00,0x42,0x42,0x42,0x7e,0x42,0x42,0x42}, // H
                     {0x00,0x7c,0x10,0x10,0x10,0x10,0x10,0x7e}, // I
                     {0x00,0x7e,0x02,0x02,0x02,0x02,0x04,0x38}, // J
                     {0x00,0x44,0x48,0x50,0x60,0x50,0x48,0x44}, // K
                     {0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x7c}, // L
                     {0x00,0x82,0xc6,0xaa,0x92,0x82,0x82,0x82}, // M
                     {0x00,0x42,0x42,0x62,0x52,0x4a,0x46,0x42}, // N
                     {0x00,0x3c,0x42,0x42,0x42,0x42,0x44,0x38}, // O
                     {0x00,0x78,0x44,0x44,0x48,0x70,0x40,0x40}, // P
                     {0x00,0x3c,0x42,0x42,0x52,0x4a,0x44,0x3a}, // Q
                     {0x00,0x78,0x44,0x44,0x78,0x50,0x48,0x44}, // R
                     {0x00,0x38,0x40,0x40,0x38,0x04,0x04,0x78}, // S
                     {0x00,0x7e,0x90,0x10,0x10,0x10,0x10,0x10}, // T
                     {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3e}, // U
                     {0x00,0x42,0x42,0x42,0x42,0x44,0x28,0x10}, // V
                     {0x80,0x82,0x82,0x92,0x92,0x92,0x94,0x78}, // W
                     {0x00,0x42,0x42,0x24,0x18,0x24,0x42,0x42}, // X
                     {0x00,0x44,0x44,0x28,0x10,0x10,0x10,0x10}, // Y
                     {0x00,0x7c,0x04,0x08,0x7c,0x20,0x40,0xfe}, // Z
                      // (the font does not contain any lower case letters. you can add your own.)
                  };    // {}, //

// This is for future use!
#if 0
byte words[5][8] = { {0x00,0x60,0x90,0x85,0x65,0x15,0x95,0x63}, // Su
                     {0x00,0x40,0x40,0x67,0x54,0x57,0x51,0x67}, // bs
                     {0x00,0x00,0x00,0x24,0x57,0x45,0x54,0x24}, // cr
                     {0x00,0x50,0x10,0x58,0x55,0x55,0x55,0x58}, // ib
                     {0x00,0x00,0x00,0xc8,0xee,0x08,0x28,0xc8} }; // er
#endif

bool loadConfig();
bool saveConfig();
void forceConfigMode();

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  // You could indicate on your screen or by an LED you are in config mode here

  // We don't want the next time the board resets to be considered a double reset
  // so we remove the flag
  drd.stop();
}

void setup() {

  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }
//  Serial.println("Removing config file.");
//  SPIFFS.remove("/config.json");
//  Serial.println("Removed.");

  // init the pixels
  pixels.begin();
  pixels.setBrightness(200);
  pixels.setPixelColor(0, 255, 255, 255);
  pixels.show();

  // init the display
  lmd.setEnabled(true);
  lmd.setIntensity(5);   // 0 = low, 10 = high

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  Serial.println("Init YouTube API");
  api = new YoutubeApi(apiKey, client);
  newapi = new YoutubeApi(apiKey, client);
  loadConfig();

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Adding an additional config on the WIFI manager webpage for the API Key and Channel ID
  WiFiManagerParameter customApiKey("apiKey", "API Key", apiKey, 50);
  WiFiManagerParameter customChannelId("channelId", "Channel ID", channelId, 35);
  wifiManager.addParameter(&customApiKey);
  wifiManager.addParameter(&customChannelId);

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    wifiManager.startConfigPortal("YouTube-Counter", "supersecret");
  } else {
    Serial.println("No Double Reset Detected");
    wifiManager.autoConnect("YouTube-Counter", "supersecret");
  }

  strcpy(apiKey, customApiKey.getValue());
  strcpy(channelId, customChannelId.getValue());

  if (shouldSaveConfig) {
    saveConfig();
  }

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  // Force Config mode if there is no API key
  if(strcmp(apiKey, "") > 0) {
    Serial.println("NOT Forcing Config Mode");
  } else {
    Serial.println("Forcing Config Mode");
    forceConfigMode();
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  drd.stop();

  pixels.setPixelColor(0, 255, 255, 255);
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, 0, 0, 255);
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, 255, 255, 255);
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, 0, 0, 255);
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, 255, 255, 255);
  pixels.show();

  // Required if you are using ESP8266 V2.5 or above
  client.setInsecure();
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson( jsonDoc, configFile );
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }


  strcpy(apiKey, jsonDoc["apiKey"]);
  strcpy(channelId, jsonDoc["channelId"]);
  api->channelStats.subscriberCount = jsonDoc["subscriberCount"];
  api->channelStats.viewCount = jsonDoc["viewCount"];
  api->channelStats.commentCount = jsonDoc["commentCount"];
  api->channelStats.videoCount = jsonDoc["videoCount"];

  return true;
}

bool saveConfig() {
  StaticJsonDocument<1024> jsonDoc;
  jsonDoc["apiKey"] = apiKey;
  jsonDoc["channelId"] = channelId;

  jsonDoc["subscriberCount"] = api->channelStats.subscriberCount;
  jsonDoc["viewCount"] = api->channelStats.viewCount;
  jsonDoc["commentCount"] = api->channelStats.commentCount;
  jsonDoc["videoCount"] = api->channelStats.videoCount;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  serializeJson(jsonDoc, configFile);
  return true;
}

void forceConfigMode() {
  Serial.println("Reset");
  WiFi.disconnect();
  Serial.println("Dq");
  delay(500);
  ESP.restart();
  delay(5000);
}

char text[50];
int len = 0;

/**
 * This draws a sprite to the given position using the width and height supplied (usually 8x8)
 */
void drawSprite( byte* sprite, int x, int y, int width, int height )
{
  // The mask is used to get the column bit from the sprite row
  byte mask = B10000000;

  for( int iy = 0; iy < height; iy++ )
  {
    for( int ix = 0; ix < width; ix++ )
    {
      lmd.setPixel(x + ix, y + iy, (bool)(sprite[iy] & mask ));

      // shift the mask by one pixel to the right
      mask = mask >> 1;
    }

    // reset column mask
    mask = B10000000;
  }
}

int temp_val = 0;
void loop() {
  Serial.println("Starting loop...");
  Serial.println(millis());
  Serial.println(api_lasttime);
  Serial.println(api_mtbs);
  if (millis() - api_lasttime > api_mtbs) {
      Serial.println("Getting stats...");
    if(newapi->getChannelStatistics(channelId))
    {
      Serial.println("---------Stats---------");
      Serial.print("Subscriber Count: ");
      Serial.println(newapi->channelStats.subscriberCount);
      Serial.print("View Count: ");
      Serial.println(newapi->channelStats.viewCount);
      Serial.print("Comment Count: ");
      Serial.println(newapi->channelStats.commentCount);
      Serial.print("Video Count: ");
      Serial.println(newapi->channelStats.videoCount);
      // Probably not needed :)
      //Serial.print("hiddenSubscriberCount: ");
      //Serial.println(api->channelStats.hiddenSubscriberCount);
      Serial.println("------------------------");

      if ((newapi->channelStats.subscriberCount != api->channelStats.subscriberCount) ||
          (newapi->channelStats.viewCount != api->channelStats.viewCount) ||
          (newapi->channelStats.commentCount != api->channelStats.commentCount) ||
          (newapi->channelStats.videoCount != api->channelStats.videoCount))
          {
              if (newapi->channelStats.subscriberCount > api->channelStats.subscriberCount)
              {
                  pixels.setPixelColor(0, 255, 255, 255);
                  pixels.show();
                  delay(500);
                  pixels.setPixelColor(0, 0, 0, 255);
                  pixels.show();
                  delay(500);
                  pixels.setPixelColor(0, 255, 255, 255);
                  pixels.show();
                  delay(500);
                  pixels.setPixelColor(0, 0, 0, 255);
                  pixels.show();
                  delay(500);
                  pixels.setPixelColor(0, 255, 255, 255);
                  pixels.show();
              }
              Serial.println("Saving Config...");
              memcpy( api, newapi, sizeof(YoutubeApi) );
              saveConfig();
              Serial.println("Done.");
          }
    } else {
        Serial.println("Failed.");
    }
    api_lasttime = millis();
  }

   // In case you wonder why we don't have to call lmd.clear() in every loop: The font has a opaque (black) background...
   //api->channelStats.subscriberCount = i++;
   //if (1) {
     snprintf( text, 50, "%ld", api->channelStats.subscriberCount );
     temp_val = api->channelStats.subscriberCount;
     len = strlen( text );
     if (temp_val > 99999)
     {
       drawSprite( font[text[len-6]-32], 0, 0, 5, 8 );
     } else {
       drawSprite( font[' '-32], 0, 0, 5, 8 );
     }
     if (temp_val > 9999)
     {
       drawSprite( font[text[len-5]-32], 5, 0, 5, 8 );
     } else {
       drawSprite( font[' '-32], 5, 0, 5, 8 );
     }
     if (temp_val > 999)
     {
       drawSprite( font[text[len-4]-32], 10, 0, 5, 8 );
       drawSprite( font[','-32], 15, 0, 5, 8 );
     } else {
       drawSprite( font[' '-32], 10, 0, 5, 8 );
       drawSprite( font[' '-32], 15, 0, 5, 8 );
     }
     if (temp_val > 99)
     {
       drawSprite( font[text[len-3]-32], 18, 0, 5, 8 );
     } else {
       drawSprite( font[' '-32], 18, 0, 5, 8 );
     }
     if (temp_val > 9)
     {
       drawSprite( font[text[len-2]-32], 23, 0, 5, 8 );
     } else {
       drawSprite( font[' '-32], 23, 0, 5, 8 );
     }
     drawSprite( font[text[len-1]-32], 28, 0, 5, 8 );
//   } else {
//     drawSprite( words[0], 0, 0, 8, 8 );
//     drawSprite( words[1], 8, 0, 8, 8 );
//     drawSprite( words[2], 16, 0, 8, 8 );
//     drawSprite( words[3], 24, 0, 8, 8 );
//   }

   // Toggle display of the new framebuffer
   lmd.display();

   // Wait between updates
   delay(api_mtbs);

   // Advance to next coordinate
   if( --x < len * -8 )
     x = LEDMATRIX_WIDTH;
 }
