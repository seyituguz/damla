#include <TFT_eSPI.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();

// WiFi Bilgileri
const char* ssid = "TP-LINK_1AB4";
const char* password = "73182207";

// OTA URL'leri
const char* versionURL = "https://raw.githubusercontent.com/seyituguz/damla/main/main/version.txt";
const char* otaURL     = "https://raw.githubusercontent.com/seyituguz/damla/main/main/firmware.bin";
String currentVersion = "1.0.0";

// Hava durumu API
String weatherURL = "http://api.open-meteo.com/v1/forecast?latitude=37.87&longitude=32.48&current_weather=true";
float temperature = 0;
String durum = "";
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 300000; // 5 dakika

// Yapılar
struct Cloud { int x, y, speed; };
Cloud clouds[] = {
  {0,  80, 1},
  {100,110, 2},
  {180,130, 1}
};

// -----------------------------
// ŞİMŞEK (Yeni basit görsel)
// -----------------------------
void drawLightning() {
  tft.fillTriangle(110, 60, 130, 100, 115, 100, TFT_YELLOW);
  tft.fillTriangle(120, 100, 140, 140, 125, 140, TFT_YELLOW);
}

// -----------------------------
// KAR (Buluttan Düşen Kar)
// -----------------------------
struct Snow { int x, y, speed; };
Snow snowFall[15];

void initSnow() {
  for (int i = 0; i < 15; i++) {
    snowFall[i].x = random(90, 150);
    snowFall[i].y = random(70, 150);
    snowFall[i].speed = random(1, 3);
  }
}

void drawSnow() {
  for (int i = 0; i < 15; i++) {
    tft.fillCircle(snowFall[i].x, snowFall[i].y, 3, TFT_WHITE);
    snowFall[i].y += snowFall[i].speed;
    if (snowFall[i].y > 220) {
      snowFall[i].y = random(80, 120);
      snowFall[i].x = random(100, 140);
    }
  }
}

// -----------------------------
// YAĞMUR (Buluttan Düşen Damla)
// -----------------------------
struct Drop { int x, y, speed; };
Drop rainDrops[20];

void initRain() {
  for (int i = 0; i < 20; i++) {
    rainDrops[i].x = random(90, 150);
    rainDrops[i].y = random(70, 150);
    rainDrops[i].speed = random(3, 7);
  }
}

void drawRain() {
  for (int i = 0; i < 20; i++) {
    tft.drawLine(rainDrops[i].x, rainDrops[i].y, rainDrops[i].x, rainDrops[i].y + 6, TFT_CYAN);
    rainDrops[i].y += rainDrops[i].speed;
    if (rainDrops[i].y > 220) {
      rainDrops[i].y = random(80, 120);
      rainDrops[i].x = random(100, 140);
    }
  }
}

// -----------------------------
// BULUT
// -----------------------------
void drawCloud(int x, int y, uint16_t color) {
  tft.fillCircle(x,     y,     12, color);
  tft.fillCircle(x+10,  y+5,   15, color);
  tft.fillCircle(x-10,  y+5,   10, color);
  tft.fillCircle(x+20,  y,     10, color);
}

// -----------------------------
// GÖKYÜZÜ
// -----------------------------
void drawSkyGradient() {
  for (int y = 0; y < 240; y++) {
    uint16_t color = tft.color565(25 + (y/10), 150 + (y/5), 255 - (y/15));
    tft.drawFastHLine(0, y, 240, color);
  }
}

// -----------------------------
// EKRAN TEMİZLEME
// -----------------------------
void screenClear() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
}

// -----------------------------
// TEST MODU (Tam animasyon seti)
// -----------------------------
void screenTest() {
  screenClear();
  tft.setTextColor(TFT_CYAN);
  tft.drawString("WRAITH SCREEN TEST", 10, 10, 2);
  delay(700);

  drawSkyGradient();
  tft.fillCircle(120,100,25,TFT_YELLOW);
  delay(700);

  drawSkyGradient();
  drawCloud(120,100,TFT_WHITE);
  delay(700);

  drawSkyGradient();
  initRain(); for(int i=0;i<12;i++){ drawRain(); delay(40);}  

  drawSkyGradient();
  drawSnow();
  delay(700);

  drawSkyGradient();
  drawLightning();
  delay(700);

  screenClear();
  tft.setTextColor(TFT_GREEN);
  tft.drawString("SCREEN [OK]", 10, 100, 2);
  delay(900);
}

// -----------------------------
// BIOS EKRANI
// -----------------------------
void biosBoot(String status, bool wifiOK) {
  static bool headerDrawn = false;

  if (!headerDrawn) {
    screenClear();
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(1);

    int y = 10;
    tft.drawString("WRAITH Technologies UEFI BIOS v2.41", 10, y); y += 12;
    tft.drawString("© 2025 WRAITH Corp.", 10, y); y += 18;
    tft.drawString("CPU: ESP8266EX @ 80MHz", 10, y); y += 12;
    tft.drawString("Memory Test: 8192KB OK", 10, y); y += 15;
    tft.drawString("Display Controller: OK", 10, y); y += 12;
    tft.drawString("Firmware: " + currentVersion, 10, y); y += 12;
    tft.drawString("WiFi: " + String(wifiOK ? "Connected [OK]" : "Failed"), 10, y); y += 20;

    headerDrawn = true;
  }

  // Yalnızca durum satırını güncelle
  tft.fillRect(10, 130, 220, 12, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Status: " + status, 10, 130);
}
// -----------------------------
// HAVA DURUMU ÇEKME
// -----------------------------
bool fetchWeather() {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, weatherURL)) return false;
  int code = http.GET();
  if (code != 200) return false;

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, http.getString())) return false;

  temperature = doc["current_weather"]["temperature"];
  int w = doc["current_weather"]["weathercode"];

  if (w == 0) durum = "Gunesli";
  else if (w <= 2) durum = "Parcali Bulutlu";
  else if (w == 3) durum = "Bulutlu";
  else if (w >= 61 && w <= 65) durum = "Yagmurlu";
  else if (w >= 71 && w <= 75) durum = "Karli";
  else if (w >= 95) durum = "Firtinali";
  else durum = "Degisken";

  return true;
}

// -----------------------------
// OTA
// -----------------------------
void checkForUpdate() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, versionURL)) return;
  int httpCode = http.GET();
  if (httpCode != 200) return;

  String newVersion = http.getString();
  newVersion.trim();
  http.end();

  if (newVersion == currentVersion) return;

  screenClear();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Yeni surum bulundu!", 120, 100, 2);

  ESPhttpUpdate.onProgress([](int cur, int total) {
    int pct = (cur * 100) / total;
    tft.fillRect(40, 130, 160, 20, TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("Guncelleme %" + String(pct), 120, 140, 2);
  });

  ESPhttpUpdate.rebootOnUpdate(false);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, otaURL, currentVersion);

  if (ret == HTTP_UPDATE_OK) ESP.restart();
}

// -----------------------------
// SETUP
// -----------------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);

  // 1) BIOS BAŞLANGIÇ
  biosBoot("Initializing...", false);
  delay(600);

  // 2) EKRAN TEST MODU
  biosBoot("Screen Test", false);
  delay(400);
  screenTest();

  // Test bitti → BIOS'a dön
  biosBoot("Screen [OK]", false);
  delay(900);

  // 3) WiFi BAĞLANTISI
  biosBoot("WiFi Connecting...", false);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
  }
  bool wifiOK = WiFi.status() == WL_CONNECTED;

  biosBoot(wifiOK ? "WiFi [OK]" : "WiFi [FAILED]", wifiOK);
  delay(1000);

  // 4) OTA KONTROLÜ
  biosBoot("Checking Update...", wifiOK);
  delay(500);
  if (wifiOK) checkForUpdate();

  biosBoot("Firmware Up-To-Date", wifiOK);
  delay(800);

  biosBoot("SYSTEM READY", wifiOK);
  delay(1200);

  // 5) HAVA DURUMU AL
  fetchWeather();

  drawSkyGradient();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(durum, 120, 40, 4);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(String(temperature, 1) + "°C", 120, 190, 6);

  initRain();
}

// -----------------------------
// LOOP
// -----------------------------
void loop() {
  // 5 dakikada bir hava durumu güncelle
  if (millis() - lastWeatherUpdate > WEATHER_INTERVAL) {
    lastWeatherUpdate = millis();
    if (fetchWeather()) {
      drawSkyGradient();
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(durum, 120, 40, 4);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(String(temperature, 1) + "°C", 120, 190, 6);
    }
  }

  // Animasyonlar
  if (durum.indexOf("Bulut") >= 0) {
    for (auto &c : clouds) {
      tft.fillRect(c.x - 25, c.y - 15, 70, 40, tft.color565(100, 180, 255));
      c.x += c.speed;
      if (c.x > 260) c.x = -30;
      drawCloud(c.x, c.y, TFT_WHITE);
    }
  }

  if (durum == "Yagmurlu") {
    drawRain();
  }

  if (durum == "Karli") {
    drawSnow();
  }

  if (durum == "Firtinali") {
    drawLightning();
  }

  delay(40);
}
