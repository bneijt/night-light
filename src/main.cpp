#include <Adafruit_NeoPixel.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Timezone.h>
#include <limits>

// === CONFIGURATION ===
#define PIXELS_PIN D2
#define NPIXELS 40
#define BRIGHTNESS 5
#define SSID "YOUR_WIFI_NAME"
#define WPA_PSK "YOUR_WIFI_KEY"
#define UPDATE_INTERVAL 60e3
#define HUE_MAX UINT_LEAST16_MAX

// Central European Summer Time
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);

int previousNightSeconds = (24 - 19) * 3600;
int halfDayInSeconds = 12 * 3600;

// === GLOBAL STATE ===
Adafruit_NeoPixel pixels =
    Adafruit_NeoPixel(NPIXELS, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

long millis_per_segment = 5 * 1000 / NPIXELS;

///////////////////////

inline void setLed(size_t ledIdx, uint16_t hue) {
  pixels.setPixelColor(ledIdx, pixels.ColorHSV(hue, 255, BRIGHTNESS));
}

void updateProgress(time_t const &t) {
  int todaySeconds = 3600 * hour(t) + 60 * minute(t) + second(t);

  int progressInDaySeconds =
      (todaySeconds + previousNightSeconds) % (halfDayInSeconds * 2);
  int progressInCircleSeconds = progressInDaySeconds % halfDayInSeconds;
  bool isDay = progressInDaySeconds > halfDayInSeconds;
  int progressInCircleIdx =
      (NPIXELS * progressInCircleSeconds) / halfDayInSeconds;
  pixels.clear();
  Serial.printf("Now %.2d:%.2d:%.2d => isDay=%d progress=%d\n", hour(t),
                minute(t), second(t), isDay, progressInCircleIdx);

  for (uint16_t ledIdx = 0; ledIdx <= progressInCircleIdx; ++ledIdx) {
    if (isDay) {
      // During the day cycle, all colors of the rainbow
      setLed(ledIdx, ledIdx * (HUE_MAX / 40));
    } else {
      // During the night cycle, yellow
      setLed(ledIdx, 7000);
    }
  }
  pixels.show();
}

bool connect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Connecting to %s ", SSID);
    WiFi.forceSleepWake();
    delay(1);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, WPA_PSK);
  }
  for (int i = 0; i < 1000; ++i) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("failed to connect");
  }
  return WiFi.status() == WL_CONNECTED;
}

String getDateHeader() {
  WiFiClient client;
  HTTPClient http;
  char const *headerKeys[] = {"date"};
  size_t const numberOfHeaders = 1;
  http.collectHeaders(headerKeys, numberOfHeaders);

  if (http.begin(client, "http://1.1.1.1/")) {
    if (http.GET() > 0) {
      String header = http.header("date");
      http.end();
      return header;
    } else {
      Serial.println("Failed to GET on http endpoint");
    }
  } else {
    Serial.println("Unable to connect to http endpoint");
  }
  http.end();
  return "";
}

time_t getTime() {
  while (true) {
    Serial.println("Attempting time update");
    if (!connect())
      continue;

    String headerDate = getDateHeader();
    if (headerDate.length() == 0)
      continue;

    // Disconnect wifi
    WiFi.disconnect(true);
    delay(1);
    WiFi.forceSleepBegin();
    delay(1);

    struct tm parsedTime;
    // Parse "Sat, 25 Jan 2020 08:34:53 GMT"
    if (strptime(headerDate.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &parsedTime) !=
        NULL) {
      return mktime(&parsedTime);
    } else {
      Serial.print("Could not parse header: ");
      Serial.println(headerDate);
    }
    delay(5000);
  }
}

void setup() {
  Serial.begin(115200);
  pixels.begin();
  setSyncProvider(getTime);
  // Sync weekly
  setSyncInterval(24 * 3600 * 7);
}

void loop() {
  time_t localTime = CE.toLocal(now());
  updateProgress(localTime);
  delay(UPDATE_INTERVAL);
}
