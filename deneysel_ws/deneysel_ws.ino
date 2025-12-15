#include <TFT_eSPI.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();

// -----------------------------------------------------------------------------
// WIFI AYARLARI
// -----------------------------------------------------------------------------
const char* ssid2 = "TP-LINK_1AB4";
const char* pass2 = "73182207";

const char* ssid1 = "Turksat_Kablonet_2.4_3C67";   // yedek WiFi
const char* pass1 = "k2UZEA49";

// -----------------------------------------------------------------------------
// OTA
// -----------------------------------------------------------------------------
const char* versionURL = "https://raw.githubusercontent.com/seyituguz/damla/main/main/version.txt";
const char* otaURL     = "https://raw.githubusercontent.com/seyituguz/damla/main/main/firmware.bin";
String currentVersion = "1.0.4";

// -----------------------------------------------------------------------------
// WEATHER API
// -----------------------------------------------------------------------------
String weatherURL = "http://api.open-meteo.com/v1/forecast?latitude=37.87&longitude=32.48&current_weather=true";
float temperature = 0;
String durum = "";
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 300000;

// OTA auto-update timer
unsigned long lastUpdateCheck = 0;
const unsigned long UPDATE_INTERVAL = 300000;

// -----------------------------------------------------------------------------
// CLOUD STRUCT
// -----------------------------------------------------------------------------
struct Cloud { int x, y, speed; };
Cloud clouds[] = {
  {0,70,1}, {100,105,2}, {180,140,1}
};

// -----------------------------------------------------------------------------
// STATUS LED (D0 & D6 arasina bagli iki renkli LED)
// D0 HIGH / D6 LOW  -> SARI (NORMAL CALISMA)
// D0 LOW  / D6 HIGH -> KIRMIZI (BIOS / OTA / HATA)
// -----------------------------------------------------------------------------
const int LED_PIN_A = D0; // birinci bacak
const int LED_PIN_B = D6; // ikinci bacak

void setLedNormal() {
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_A, LOW);
}

void setLedUpdate() {
  digitalWrite(LED_PIN_B, LOW);
  digitalWrite(LED_PIN_A, HIGH);
}

// -----------------------------------------------------------------------------
// CLOUD DRAW
// -----------------------------------------------------------------------------
void drawCloud(int x, int y, uint16_t color) {
  tft.fillCircle(x, y, 12, color);
  tft.fillCircle(x+10, y+5, 15, color);
  tft.fillCircle(x-10, y+5, 10, color);
  tft.fillCircle(x+20, y, 10, color);
}

// -----------------------------------------------------------------------------
// SUN
void drawSun() {
  tft.fillCircle(120,100,25,TFT_YELLOW);
  for(int i=0;i<8;i++){
    float a = i * 45 * 0.01745;
    int x2 = 120 + cos(a)*40;
    int y2 = 100 + sin(a)*40;
    tft.drawLine(120,100,x2,y2,TFT_YELLOW);
  }
}

// LIGHTNING
// -----------------------------------------------------------------------------
void drawLightning() {
  drawCloud(120, 90, TFT_WHITE);
  tft.fillTriangle(115,110, 130,150, 120,150, TFT_YELLOW);
  tft.fillTriangle(125,90, 140,130, 130,130, TFT_YELLOW);
}

// -----------------------------------------------------------------------------
// SNOW
// -----------------------------------------------------------------------------
struct Snow { int x, y, speed; };
Snow snowFall[15];

void initSnow() {
  for (int i=0;i<15;i++){
    snowFall[i].x = random(100,140);
    snowFall[i].y = random(80,140);
    snowFall[i].speed = random(1,3);
  }
}

void drawSnow() {
  drawCloud(120,90,TFT_WHITE);
  for (int i=0;i<15;i++){
    tft.fillCircle(snowFall[i].x, snowFall[i].y, 3, TFT_WHITE);
    snowFall[i].y += snowFall[i].speed;
    if(snowFall[i].y > 220){
      snowFall[i].y = random(80,120);
      snowFall[i].x = random(100,140);
    }
  }
}

// -----------------------------------------------------------------------------
// RAIN
// -----------------------------------------------------------------------------
struct Drop { int x,y,speed; };
Drop rainDrops[20];

void initRain(){
  for(int i=0;i<20;i++){
    rainDrops[i].x = random(100,140);
    rainDrops[i].y = random(80,140);
    rainDrops[i].speed = random(3,7);
  }
}

void drawRain(){
  drawCloud(120,90,TFT_WHITE);
  for(int i=0;i<20;i++){
    tft.drawLine(rainDrops[i].x, rainDrops[i].y, rainDrops[i].x, rainDrops[i].y+6, TFT_CYAN);
    rainDrops[i].y += rainDrops[i].speed;
    if(rainDrops[i].y > 220){
      rainDrops[i].y = random(80,120);
      rainDrops[i].x = random(100,140);
    }
  }
}

// -----------------------------------------------------------------------------
// SKY BACKGROUND
// -----------------------------------------------------------------------------
void drawSkyGradient() {
  for(int y=0;y<240;y++){
    uint16_t c = tft.color565(25+(y/10), 150+(y/5), 255-(y/15));
    tft.drawFastHLine(0,y,240,c);
  }
}

// -----------------------------------------------------------------------------
// CLEAR SCREEN
// -----------------------------------------------------------------------------
void screenClear() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
}

// -----------------------------------------------------------------------------
// BIOS (Yeni, gerçekçi, akışlı BIOS)
// -----------------------------------------------------------------------------
int biosLine = 10;

void biosBoot(String status){
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);

  if(biosLine==10){
    screenClear();

    // Üstte mavi BIOS header barı
    tft.fillRect(0,0,240,18, tft.color565(10,40,120));
    tft.setTextColor(TFT_CYAN);
    tft.drawString("              BIOS v3.10", 6, 4);

    biosLine = 28;
    delay(500);
    // Sistem bilgileri - daha gerçekçi format
    tft.setTextColor(TFT_WHITE);
    tft.drawString("     Processor __ ESP8266 @80MHz", 10, biosLine); biosLine+=14;
    delay(200);
    tft.drawString("   Memory Test ___ 8192 KB", 10, biosLine); biosLine+=14;
    delay(200);
    tft.drawString("  Video Adapter ___ TFT_eSPI", 10, biosLine); biosLine+=14;
    delay(200);
    tft.drawString("Firmware Version ___ "+currentVersion, 10, biosLine); biosLine+=14;
    delay(200);
    tft.drawString("Boot Services _______ Initializing...", 10, biosLine); biosLine+=18;
    // Ayırıcı çizgi
    tft.drawFastHLine(0, biosLine, 240, TFT_DARKGREY);
    delay(500);
    biosLine+=12;
  }

  // Akış halinde alt satıra yazılan durum
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(status, 10, biosLine);
  biosLine += 14;
  delay(500);
}

// -----------------------------------------------------------------------------
// WEATHER FETCH
// -----------------------------------------------------------------------------
bool fetchWeather(){
  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if(!http.begin(client,weatherURL)) return false;
  int code=http.GET(); if(code!=200) return false;

  DynamicJsonDocument doc(2048);
  if(deserializeJson(doc,http.getString())) return false;

  temperature = doc["current_weather"]["temperature"];
  int w = doc["current_weather"]["weathercode"];

  if (w==0) durum="Gunesli";
  else if (w<=2) durum="Parcali Bulutlu";
  else if (w==3) durum="Bulutlu";
  else if (w>=61 && w<=65) durum="Yagmurlu";
  else if (w>=71 && w<=75) durum="Karli";
  else if (w>=95) durum="Firtinali";
  else durum="Degisken";

  return true;
}

// -----------------------------------------------------------------------------
// OTA UPDATE
// -----------------------------------------------------------------------------
void checkForUpdate(){
  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;

  // GitHub OTA fix — chunked encoding and TLS issues
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if(!http.begin(client, versionURL)) return;
  int code = http.GET();
  if(code != 200){ http.end(); return; }

  String newVer = http.getString();
  newVer.trim();
  http.end();

  if(newVer.length() < 3) return;
  if(newVer == currentVersion) return;

  screenClear(); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Yeni surum bulundu!",120,100,2);

  ESPhttpUpdate.onProgress([](int cur,int tot){
    int pct = (cur * 100) / tot;
    tft.fillRect(40,130,160,20,TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("Guncelleme %" + String(pct),120,140,2);
  });

  ESPhttpUpdate.rebootOnUpdate(false);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, otaURL, currentVersion);

  if(ret == HTTP_UPDATE_OK) ESP.restart();
}


// -----------------------------
// SETUP
// -----------------------------
void setup(){
  Serial.begin(115200);
  tft.init(); tft.setRotation(0);

  // LED pinleri
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);

  // Ilk acilis: BIOS / OTA modunda kirmizi
  setLedUpdate();

  biosBoot("Initializing..."); delay(600);

  biosBoot("WiFi Connecting...");
  WiFi.begin(ssid1, pass1);

  // 8 saniye içinde baglanamazsa ikinci ağa geç
  unsigned long wifiTry = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - wifiTry < 8000) delay(200);

  if(WiFi.status() != WL_CONNECTED) {
    biosBoot("Trying Backup WiFi...");
    WiFi.begin(ssid2, pass2);
  }
  unsigned long t=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t<8000) delay(200);

  biosBoot(WiFi.status()==WL_CONNECTED ? "WiFi Connection [OK]" : "WiFi Connection [FAILED]"); delay(600);

  if(WiFi.status()==WL_CONNECTED){
    biosBoot(" Checking Update..."); delay(500);
    checkForUpdate();
  }

  biosBoot("   Firmware Up-To-Date"); delay(600);
  biosBoot("    SYSTEM READY !"); delay(1000);

  // Artik sistem normal modda, LED sari
  setLedNormal();

  fetchWeather();

  drawSkyGradient();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(durum,120,40,4);
  if(durum=="Gunesli") drawSun();
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(String(temperature,1),120,190,6);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(String ("Derece"),120,220,4);
  initRain();
  initSnow();
}

// -----------------------------------------------------------------------------
// WIFI SIGNAL LEVEL (RSSI)
// -----------------------------------------------------------------------------
int wifiLevel = 0;
unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 3000;

void drawWifiIndicator(int level) {
  // level = 0–3
  int x = 90, y = 0;
  for(int i=0;i<4;i++){
    uint16_t col = (i <= level ? TFT_GREEN : TFT_DARKGREY);
    tft.fillRect(x + i*10, y + (15 - i*4), 8, i*4, col);
  }
}

// -----------------------------------------------------------------------------
// NORMAL MODE LED BLINK (YEŞİL)
// -----------------------------------------------------------------------------
bool ledState = false;
unsigned long lastLedBlink = 0;
const unsigned long LED_BLINK_INTERVAL = 2000; // 1 saniye

// -----------------------------
// LOOP
// -----------------------------
void loop(){
  // --- Hava durumu güncellemesi ---
  if(millis() - lastWeatherUpdate > WEATHER_INTERVAL){
    lastWeatherUpdate = millis();
    if(fetchWeather()){
      drawSkyGradient();
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(durum,120,40,4);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(String(temperature,1),120,190,6);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(String ("Derece"),120,220,4);
      drawWifiIndicator(wifiLevel);
    }
  }

  // --- OTA her 5 dakikada bir kontrol ---
  if(millis() - lastUpdateCheck > UPDATE_INTERVAL){
    lastUpdateCheck = millis();
    setLedUpdate();
    checkForUpdate();
    setLedNormal();
  }

  // --- WiFi sinyal seviyesi ölç ---
  if(millis() - lastWifiCheck > WIFI_CHECK_INTERVAL){
    lastWifiCheck = millis();
    long rssi = WiFi.RSSI();
    if(rssi > -60) wifiLevel = 3;
    else if(rssi > -70) wifiLevel = 2;
    else if(rssi > -80) wifiLevel = 1;
    else wifiLevel = 0;
    drawWifiIndicator(wifiLevel);
  }

  // --- LED yeşil yanıp sönme (normal mod) ---
  if(millis() - lastLedBlink > LED_BLINK_INTERVAL){
    lastLedBlink = millis();
    ledState = !ledState;
    if(ledState) setLedNormal(); else { digitalWrite(LED_PIN_A, LOW); digitalWrite(LED_PIN_B, LOW); }
  }

  // --- Hava durumu animasyonları ---
  if(durum.indexOf("Bulut")>=0){
    for(auto &c : clouds){
      tft.fillRect(c.x-25, c.y-15, 70,40, tft.color565(100,180,255));
      c.x+=c.speed;
      if(c.x>260) c.x=-30;
      drawCloud(c.x,c.y,TFT_WHITE);
    }
  }

  if(durum=="Gunesli") drawSun();
  if(durum=="Yagmurlu") drawRain();
  if(durum=="Karli") drawSnow();
  if(durum=="Firtinali") drawLightning();

  delay(40);
}
