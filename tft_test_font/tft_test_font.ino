#include <TFT_eSPI.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

TFT_eSPI tft = TFT_eSPI();

// WiFi bilgileri
const char* ssid = "TP-LINK_1AB4";
const char* password = "73182207";

// Open-Meteo API (Konya koordinatları, HTTP ile ve timezone sabit)
String weatherURL = "http://api.open-meteo.com/v1/forecast?latitude=37.87&longitude=32.48&current_weather=true&hourly=precipitation&timezone=Europe/Istanbul";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000); // GMT+3

String daysOfWeek[7] = {"Pazartesi", "Sali", "Carsamba", "Persembe", "Cuma", "Cumartesi", "Pazar"};

#define TFT_GREY 0x8410

// Türkçe karakter düzeltme
String fixTurkish(String text) {
  text.replace("ç", "c"); text.replace("Ç", "C");
  text.replace("ğ", "g"); text.replace("Ğ", "G");
  text.replace("ı", "i"); text.replace("İ", "I");
  text.replace("ö", "o"); text.replace("Ö", "O");
  text.replace("ş", "s"); text.replace("Ş", "S");
  text.replace("ü", "u"); text.replace("Ü", "U");
  return text;
}

// Ekranda ortalı yazı
void drawTextCenter(int16_t y, String text, uint16_t color, int textSize = 2) {
  text = fixTurkish(text);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(textSize);
  int16_t x = (tft.width() - tft.textWidth(text)) / 2;
  tft.drawString(text, x, y);
}

// Mesaj gösterme
void showMessage(String msg, uint16_t color = TFT_WHITE) {
  tft.fillScreen(TFT_BLACK);
  drawTextCenter(120, msg, color);
  Serial.println(msg);
}

// Hava durumu ikonu çizme
void drawWeatherIcon(int code) {
  int y = 40; // ikon biraz yukarı
  tft.fillRect(70, y-10, 100, 70, TFT_BLACK); // önce temizle
  switch (code) {
    case 0: tft.fillCircle(120, y, 25, TFT_YELLOW); break; // Gunesli
    case 1: case 2: // Parçali bulutlu
      tft.fillCircle(120, y, 25, TFT_YELLOW);
      tft.fillCircle(140, y + 10, 20, TFT_GREY);
      break;
    case 3: // Bulutlu
      tft.fillCircle(120, y + 10, 25, TFT_GREY);
      break;
    case 61: case 63: case 65: // Yagmurlu
      tft.fillCircle(120, y + 10, 25, TFT_GREY);
      for (int i = 0; i < 3; i++) tft.fillCircle(100 + i * 20, y + 40, 5, TFT_BLUE);
      break;
    case 71: case 73: case 75: // Karli
      tft.fillCircle(120, y + 10, 25, TFT_GREY);
      for (int i = 0; i < 3; i++) tft.drawLine(100 + i * 15, y + 35, 105 + i * 15, y + 45, TFT_WHITE);
      break;
    default:
      tft.fillCircle(120, y, 25, TFT_DARKGREY); break;
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  showMessage("Sistemi Baslatiyor", TFT_CYAN);
  delay(1000);

  WiFi.begin(ssid, password);
  showMessage("WiFi Baglaniyor...", TFT_CYAN);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    showMessage("WiFi Bağlandı!", TFT_GREEN);
  } else {
    showMessage("WiFi Baglanamadi!", TFT_RED);
    return;
  }

  timeClient.begin();
  timeClient.update();
  delay(1500);
  showMessage("Veri Yukleniyor...", TFT_YELLOW);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi Koptu!", TFT_RED);
    delay(5000);
    return;
  }

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, weatherURL)) {
    showMessage("HTTP Baslatma Hatasi!", TFT_RED);
    delay(5000);
    return;
  }

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float tempC = doc["current_weather"]["temperature"].as<float>();
      float wind = doc["current_weather"]["windspeed"].as<float>();
      int code = doc["current_weather"]["weathercode"].as<int>();
      float precipitation = doc["hourly"]["precipitation"][0].as<float>();

      timeClient.update();
      time_t rawTime = timeClient.getEpochTime();
      struct tm* ti = localtime(&rawTime);
      int dayIndex = ti->tm_wday - 1;
      if (dayIndex < 0) dayIndex = 6;

      char dateStr[20];
      sprintf(dateStr, "%02d/%02d/%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);

      tft.fillScreen(TFT_BLACK);
//      drawTextCenter(10, "Konya", TFT_CYAN, 2);

      drawWeatherIcon(code);

      drawTextCenter(100, "Sıcaklık: " + String(tempC) + "C", TFT_WHITE);
      drawTextCenter(130, "Rüzgar: " + String(wind) + " km/s", TFT_WHITE, 2);
      drawTextCenter(160, "Yağış: " + String(precipitation) + " mm", TFT_WHITE, 2);
      drawTextCenter(190, daysOfWeek[dayIndex], TFT_GREEN, 2);
      drawTextCenter(210, dateStr, TFT_GREEN, 2);
    } else {
      showMessage("JSON Cozumleme Hatasi!", TFT_RED);
      Serial.println(payload);
    }
  } else {
    showMessage( "SYSTEM:", TFT_CYAN);
    showMessage("HTTP Hata: " + String(httpCode), TFT_RED);
  }

  http.end();
  delay(15000);
}
