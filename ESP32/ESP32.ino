#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <rdm6300.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <Preferences.h>

#define RDM6300_RX_PIN 4  // read the SoftwareSerial doc
#define WAKEUP_PIN GPIO_NUM_4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// untuk nyimpan uid yang mau didaftarin
char latestuid[7];
// esp awake time default 10 detik untuk scan uid yang sudah terdaftar
unsigned long timeawake = 10000000;  // 1000000 = 1 detik

Rdm6300 rdm6300;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);
Preferences uid;
int64_t startTime;

// tampilan website
void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

// fungsi memasukkan produk baru
void newproductfunc() {
  if (server.hasArg("product-name") && server.hasArg("newid")) {
    String productName = server.arg("product-name");
    char newid[7];
    server.arg("newid").toCharArray(newid, 7);

    uid.putString(newid, productName);
    server.send(200, "text/plain", "Product added: " + productName + " with ID: " + newid);
    Serial.print(newid);
    Serial.print(",");
    Serial.println(uid.getString(newid));
    sleep();
    memset(latestuid, 0, sizeof(latestuid));
    startTime = esp_timer_get_time();
    timeawake = 10000000;  // Sleep setelah 10 detik
  } else {
    server.send(200, "text/plain", "Unable to store the new product!");
    return;
  }
}


// fungsi delete produk
void deleteproduct() {
  while (Serial.available() > 0) {
    String deleteid = Serial.readStringUntil('\n'); // optional ,
    deleteid.trim();
    // if (uid.isKey(deleteid.c_str())) {
    uid.remove(deleteid.c_str());
    // } else {
    //   Serial.println("Not WORK!!!!!!!!!!!!");
    // }
  }
}

// fungsi mencari rfid yang terdaftar
void searchfunc(String id) {
  if (uid.isKey(id.c_str())) {
    // Serial.print("old UID: ");  // debug
    Serial.println(id);

    olduid(id.c_str());

    startTime = esp_timer_get_time();
    timeawake = 10000000;
  } else {
    // Serial.print("new UID: ");  // debug
    // Serial.println(id);         // debug

    newuid();
    memset(latestuid, 0, sizeof(latestuid));
    id.toCharArray(latestuid, 7);
    startTime = esp_timer_get_time();
    timeawake = 300000000;  // awake for 5 menit untuk scan uid yang belum terdaftar
  }
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  rdm6300.begin(RDM6300_RX_PIN);
  uid.begin("back", false); // inisiasi preference dengan read write mode

  // inisisasi esp32 sebagai Access Point
  WiFi.softAP("PIEC");
  IPAddress IP = WiFi.softAPIP();
  // Serial.print("Access Point IP Address: ");
  // Serial.println(IP);

  // inisiasi oled 128x64
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
    // Don't proceed, loop forever
  }

  // inisiasi route website
  server.on("/", handleRoot);
  server.on("/submit", newproductfunc);
  server.on("/cancel", []() {
    memset(latestuid, 0, sizeof(latestuid));
    startTime = esp_timer_get_time();
    timeawake = 10000000;
  });
  // kirim uid yang tidak terdaftar ke website
  server.on("/get-uid", []() {
    server.send(200, "text/plain", latestuid);
  });
  server.begin();


  // void loop
  while (esp_timer_get_time() - startTime < timeawake) {
    server.handleClient();
    if (rdm6300.get_new_tag_id()) {
      searchfunc(String(rdm6300.get_tag_id(), HEX));
      startTime = esp_timer_get_time();
      deleteproduct();
    }
  }

  // inisiasi deep sleep dan wake up dengan rfid scan
  uid.end();
  server.stop();
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
  memset(latestuid, 0, sizeof(latestuid));
  Serial.println("Sleep");  // debug
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  esp_deep_sleep_start();
}

void loop() {
}

// Template SCREEN OLED

void olduid(const char* name) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(NULL);
  display.setCursor(27, 6);
  display.println("PRODUCT NAME");
  display.setTextSize(2);
  display.setCursor(5, 22);
  display.println(uid.getString(name));
  display.display();
}

void newuid() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(NULL);
  display.setCursor(15, 6);
  display.println("New UID Detected!");
  display.setTextSize(1);
  display.setCursor(19, 22);
  display.println("Connect to PIEC");
  display.setTextSize(1);
  display.setCursor(14, 37);
  display.println("Visit 192.168.4.1");
  display.setTextSize(1);
  display.setCursor(4, 50);
  display.println("to register new prod");
  display.display();
}

void sleep() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(NULL);
  display.setCursor(2, 28);
  display.println("Sleep after 10 second");
  display.display();
}
