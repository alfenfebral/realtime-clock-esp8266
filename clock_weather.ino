// simplestesp8266clock.ino
//
// Libraries needed:
//  Time.h & TimeLib.h:  https://github.com/PaulStoffregen/Time
//  Timezone.h: https://github.com/JChristensen/Timezone
//  SSD1306.h & SSD1306Wire.h:  https://github.com/squix78/esp8266-oled-ssd1306
//  NTPClient.h: https://github.com/arduino-libraries/NTPClient
//  ESP8266WiFi.h & WifiUDP.h: https://github.com/ekstrand/ESP8266wifi
//
// 128x64 OLED pinout:
// GND goes to ground
// Vin goes to 3.3V
// Data to I2C SDA (GPIO 0)
// Clk to I2C SCL (GPIO 2)

#include <ESP8266WiFi.h>
#include <WifiUDP.h>
#include <String.h>
#include <Wire.h>
#include <SSD1306.h>
#include <SSD1306Wire.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <OLEDDisplayUi.h>
// Include custom images
#include "images.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "icons.h"

// Define NTP properties
#define NTP_OFFSET 0                  // In seconds
#define NTP_INTERVAL 60 * 1000        // In miliseconds
#define NTP_ADDRESS "ca.pool.ntp.org" // change this to whatever pool is closest (see ntp.org)

#define SDA 4                // GPIO(4) D2
#define SCL 5                // GPIO(5) D1
#define DISPLAY_ADDRESS 0x3c // 0x3d for the Adafruit 1.3" OLED, 0x3C being the usual address of the OLED

// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Create a display object
SSD1306 display(DISPLAY_ADDRESS, SDA, SCL); // 0x3d for the Adafruit 1.3" OLED, 0x3C being the usual address of the OLED

OLEDDisplayUi ui(&display);

const char *ssid = "BLACK ANGEL 1";     // insert your own ssid
const char *password = "blackangel123"; // and password
String date;
String t;
const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *ampm[] = {"AM", "PM"};

int screenW = 128;
int screenH = 64;
int clockCenterX = screenW / 2;
int clockCenterY = ((screenH - 16) / 2) + 16; // top yellow part is 16 px height

// OpenWeatherMap API key and city
const String apiKey = "8f2880f4146d21a1ca44fae2e30bca26";
const String city = "Bogor";
const String countryCode = "ID"; // Use your country's ISO code

// OpenWeatherMap endpoint
const String weatherEndpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";

float temperature;
int humidity;

void screnOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
}

void screen1Frame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  date = ""; // clear the variables
  t = "";

  // update the NTP client and get the UNIX UTC timestamp
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  // convert received time stamp to time_t object
  time_t local, utc;
  utc = epochTime;

  // Define the timezone for Asia/Jakarta (UTC+7)
  TimeChangeRule asiaJakarta = {"WIB", Last, Sun, Mar, 1, 420}; // UTC + 7 hours
  Timezone jakarta(asiaJakarta, asiaJakarta);                   // Both rules are the same because there's no DST in Jakarta

  // Convert UTC to local time
  local = jakarta.toLocal(utc);

  date = getDate(local);
  t = getTime(local);

  // Display the date and time
  // Serial.println("");
  // Serial.print("Local date: ");
  // Serial.print(date);
  // Serial.println("");
  // Serial.print("Local time: ");
  // Serial.print(t);

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(clockCenterX + x, clockCenterY + y - 24, t);
  display->setFont(ArialMT_Plain_10);
  display->drawString(clockCenterX + x, clockCenterY + y, date);
}

void screen2Frame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  int temperatureInt = (int)temperature; // Convert to integer (truncates decimal part)

  // Temperature label
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(clockCenterX + x - 28, clockCenterY + y, "Temperature");

  // Temperature value
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(clockCenterX + x - 24, clockCenterY + y - 22, String(temperatureInt) + "C");

  // Humidity label
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(clockCenterX + x + 28, clockCenterY + y, "Humidity");

  // Humidity value
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(clockCenterX + x + 32, clockCenterY + y - 24, String(humidity) + "%");
}

// void screen3Frame(OLEDDisplay* display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
//   display->drawXbm(clockCenterX + x - 30, clockCenterY + y - 30, 60, 60, cat_bits);
// }

FrameCallback frames[] = {screen1Frame, screen2Frame};
// FrameCallback frames[] = { screen1Frame, screen2Frame, screen3Frame };
int frameCount = 2;
// int frameCount = 3;
OverlayCallback overlays[] = {screnOverlay};
int overlaysCount = 1;

void setup()
{
  Serial.begin(9600); // most ESP-01's use 115200 but this could vary
  timeClient.begin(); // Start the NTP UDP client

  initUI();
  connectWifi();
  fetchWeatherData();

  delay(1000);
}

void initUI()
{
  Wire.pins(SDA, SCL);  // Start the OLED with GPIO 4 and 5 on ESP-01
  Wire.begin(SDA, SCL); // 0=sda, 2=scl

  // The ESP is capable of rendering 60fps in 80Mhz mode
  // but that won't give you much time for anything else
  // run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(1);

  // Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(TOP);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();
}

void connectWifi()
{
  // Set the hostname
  const char *hostname = "Node1 ESP8266"; // Replace with your desired device name
  WiFi.hostname(hostname);

  // Connect to wifi
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  display.drawString(0, 10, "Connecting to Wifi...");
  display.display();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi at ");
  Serial.print(WiFi.localIP());
  Serial.println("");
  display.drawString(0, 24, "Connected.");
  display.display();
}

void reconnect()
{
  display.clear();
  display.drawString(0, 10, "Connecting to Wifi...");
  display.display();
  WiFi.begin(ssid, password);
  display.drawString(0, 24, "Connected.");
  display.display();
  delay(1000);
}

void loop()
{
  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0)
  {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
}

// utility function for digital clock display: prints leading 0
String twoDigits(int digits)
{
  if (digits < 10)
  {
    String i = '0' + String(digits);
    return i;
  }
  else
  {
    return String(digits);
  }
}

String getTime(time_t local)
{
  t = "";

  // format the time to 12-hour format with AM/PM and no seconds
  t += hourFormat12(local);
  t += ":";
  t += twoDigits(minute(local));
  t += ":";
  t += twoDigits(second(local));
  t += " ";
  t += ampm[isPM(local)];

  return t;
}

String getDate(time_t local)
{
  date = "";

  // now format the Time variables into strings with proper names for month, day etc
  date += days[weekday(local) - 1];
  date += ", ";
  date += months[month(local) - 1];
  date += " ";
  date += day(local);
  date += ", ";
  date += year(local);

  return date;
}

void parseWeatherData(String payload)
{
  // Parse JSON response
  DynamicJsonDocument doc(1024); // Adjust size as needed

  DeserializationError error = deserializeJson(doc, payload);
  if (!error)
  {
    float temp = doc["main"]["temp"];  // Extract temperature
    int hum = doc["main"]["humidity"]; // Extract humidity

    temperature = temp;
    humidity = hum;

    // Print temperature and humidity
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }
  else
  {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
  }
}

// Fetch weather data from API
void fetchWeatherData()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client; // Create a WiFiClient instance
    HTTPClient http;

    if (http.begin(client, weatherEndpoint))
    {                            // Updated to use WiFiClient
      int httpCode = http.GET(); // Make the request

      if (httpCode > 0)
      { // Check for the returning code
        if (httpCode == HTTP_CODE_OK)
        {
          String payload = http.getString();
          Serial.println("Weather data received");
          parseWeatherData(payload);
        }
      }
      else
      {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpCode);
      }

      http.end(); // Free resources
    }
    else
    {
      Serial.println("Unable to connect to endpoint.");
    }
  }
  else
  {
    Serial.println("WiFi not connected!");
  }
}