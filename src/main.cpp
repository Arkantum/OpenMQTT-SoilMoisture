//////----------Declaration Librarie----------//////

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <SPIFFS.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncElegantOTA.h>
#include <EEPROM.h>
#include <Tone32.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint8_t temprature_sens_read();
}

const char *ssid = "OpenMQTT";
const char *password = "";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

const int Seconde = 1000;
const int Minute = 60 * Seconde;
const int Heure = 60 * Minute;

String TempsActuel = "00:00:00";
String HeureActuel = "00";
String MinuteActuel = "00";
String SecondeActuel = "00";

int resultHeure;
int resultMinute;
int resultSeconde;

#define LED_R 14
#define LED_G 15
#define BUZZER 26

char data[100];

const char *fileconfig = "/config/config.json"; //config file
const size_t capacityConfig = 2 * JSON_ARRAY_SIZE(1000);

AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager wifiManager(&server, &dns);
WiFiClientSecure client;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

UniversalTelegramBot *bot;

Adafruit_SSD1306 display(128, 64, &Wire);

#define Pin_DHT11 22

DHT dht(Pin_DHT11, DHT11);

File root = SPIFFS.open("/");
File file = root.openNextFile();

struct StructConfig
{
  String mqttServer;
  int mqttPort = 1883;

  String hostname = "Mycodo";

  String Telegram_ID_1;
  String Telegram_ID_2;
  String Telegram_ID_3;
  String Telegram_ID_4;
  String Telegram_ID_5;
  String Telegram_TOKEN;

  String Topic_DHT11_Temperature;
  String Topic_DHT11_Humidity;
  String Topic_SoilMoisture;
};

StructConfig Configuration;

const char *INPUT_mqttip = "mqttip";
const char *INPUT_hostname = "hostname";
const char *INPUT_telegramID1 = "telegramID1";
const char *INPUT_telegramID2 = "telegramID2"; //get value html
const char *INPUT_telegramID3 = "telegramID3";
const char *INPUT_telegramID4 = "telegramID4";
const char *INPUT_telegramID5 = "telegramID5";
const char *INPUT_telegramToken = "telegramToken";
const char *INPUT_Topic_DHT11_Temperature = "DHT11Temperature";
const char *INPUT_Topic_DHT11_Humidity = "DHT11Humidity";
const char *INPUT_Topic_SoilMoisture = "SoilMoisture";

float DHT_Te;
float DHT_Hu;
float SoilMoisture_Value = map(analogRead(32), 0, 4096, 100, 0);

unsigned long TestRoutinePrecedente = 0;
const long IntervalRoutine = 1000;

void setupMQTT()
{
  mqttClient.setServer(Configuration.mqttServer.c_str(), Configuration.mqttPort);
}

void reconnect()
{
  Serial.println("Connecting to MQTT Broker...");
  if (!mqttClient.connected())
  {
    Serial.println("Reconnecting to MQTT Broker..");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("Connected.");
    }
  }
}

void ActualisationTempsServeur()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    TempsActuel = "Erreur temps !";
    return;
  }
  HeureActuel = timeinfo.tm_hour;
  if (timeinfo.tm_hour < 10)
  {
    HeureActuel = "0" + HeureActuel;
  }
  MinuteActuel = timeinfo.tm_min;
  if (timeinfo.tm_min < 10)
  {
    MinuteActuel = "0" + MinuteActuel;
  }
  SecondeActuel = timeinfo.tm_sec;
  if (timeinfo.tm_sec < 10)
  {
    SecondeActuel = "0" + SecondeActuel;
  }
  TempsActuel = HeureActuel + ":" + MinuteActuel + ":" + SecondeActuel;
  resultHeure = HeureActuel.toInt();
  resultMinute = MinuteActuel.toInt();
  resultSeconde = SecondeActuel.toInt();
}

void playSuccess()
{
  tone(BUZZER, NOTE_FS5, 100, 0);
  tone(BUZZER, NOTE_E5, 100, 0);
  tone(BUZZER, NOTE_E5, 100, 0);
}

void Message_Recu(int NombreMessagesRecu)
{
  for (int i = 0; i < NombreMessagesRecu; i++)
  {
    // Chat id of the requester
    String chat_id = String(bot->messages[i].chat_id);
    if (chat_id != Configuration.Telegram_ID_1 && chat_id != Configuration.Telegram_ID_2 && chat_id != Configuration.Telegram_ID_3 && chat_id != Configuration.Telegram_ID_4 && chat_id != Configuration.Telegram_ID_5)
    {
      bot->sendMessage(chat_id, "Utilisateur non enregistrée", "");
      continue;
    }

    // Print the received message
    String text = bot->messages[i].text;
    Serial.println(text);

    String from_name = bot->messages[i].from_name;

    if (text == "/start")
    {
      String keyboardJson = "[[\"/Info\", \"/Recap\"],[\"/Hygrometrie\"],[\"/Température\"],[\"/Pression\"]]";
      bot->sendMessageWithReplyKeyboard(chat_id, "Que voulez vous ?", "", keyboardJson, true);
    }

    if (text == "/Temperature")
    {
      String Temperature = "Température : \n\n";
      Temperature += "DHT22    :  " + String(DHT_Te) + " ºC \n";
      bot->sendMessage(chat_id, Temperature, "");
    }

    if (text == "/Hygrometrie")
    {
      String Hygrometrie = "Hygrométrie : \n\n";
      Hygrometrie += "DHT22    :  " + String(DHT_Hu) + " % \n";
      bot->sendMessage(chat_id, Hygrometrie, "");
    }

    if (text == "/Recap")
    {
      String Recap = "Recap : \n\n";
      Recap += "Température   :  " + String(DHT_Te) + " °C \n";
      Recap += "Humidité      :  " + String(DHT_Hu) + " % \n";
      Recap += "Soil Moisture :  " + String(SoilMoisture_Value) + "\n";
      bot->sendMessage(chat_id, Recap, "");
    }

    if (text == "/Info")
    {
      String Info = "Informations : \n\n";
      Info += "Adresse IP locale : " + String(WiFi.localIP()) + " \n";
      Info += "Adresse MAC locale : " + String(WiFi.macAddress()) + " \n";
      Info += "Adresse IP Mycodo : " + String(Configuration.mqttServer) + " \n";
      Info += "Hostname : " + String(Configuration.hostname) + " \n";
      bot->sendMessage(chat_id, Info, "");
    }
  }
}

void playFailed()
{
  tone(BUZZER, NOTE_FS5, 100, 0);
  tone(BUZZER, NOTE_D5, 100, 0);
  tone(BUZZER, NOTE_D5, 100, 0);
}

void Chargement()
{
  File file = SPIFFS.open(fileconfig, "r");
  if (!file)
  {
    //fichier config absent
    Serial.println("Fichier Config absent");
  }
  DynamicJsonDocument docConfig(capacityConfig);
  DeserializationError err = deserializeJson(docConfig, file);
  if (err)
  {
    Serial.print(F("deserializeJson() avorté, problème rencontré : "));
    Serial.println(err.c_str());
  }
  Configuration.mqttServer = docConfig["mqttServer"] | "";
  Configuration.hostname = docConfig["hostname"] | "";
  Configuration.Telegram_ID_1 = docConfig["Telegram_ID_1"] | "";
  Configuration.Telegram_ID_2 = docConfig["Telegram_ID_2"] | "";
  Configuration.Telegram_ID_3 = docConfig["Telegram_ID_3"] | "";
  Configuration.Telegram_ID_4 = docConfig["Telegram_ID_4"] | "";
  Configuration.Telegram_ID_5 = docConfig["Telegram_ID_5"] | "";
  Configuration.Telegram_TOKEN = docConfig["Telegram_TOKEN"] | "";
  Configuration.Topic_DHT11_Temperature = docConfig["Topic_DHT11_Temperature"] | "";
  Configuration.Topic_DHT11_Humidity = docConfig["Topic_DHT11_Humidity"] | "";
  Configuration.Topic_SoilMoisture = docConfig["Topic_SoilMoisture"] | "";
  file.close();
  bot = new UniversalTelegramBot(Configuration.Telegram_TOKEN, client);
}

void Sauvegarde()
{
  String jsondoc = "";
  DynamicJsonDocument docConfig(capacityConfig);
  docConfig["mqttServer"] = Configuration.mqttServer;
  docConfig["hostname"] = Configuration.hostname;
  docConfig["Telegram_ID_1"] = Configuration.Telegram_ID_1;
  docConfig["Telegram_ID_2"] = Configuration.Telegram_ID_2;
  docConfig["Telegram_ID_3"] = Configuration.Telegram_ID_3;
  docConfig["Telegram_ID_4"] = Configuration.Telegram_ID_4;
  docConfig["Telegram_ID_5"] = Configuration.Telegram_ID_5;
  docConfig["Telegram_TOKEN"] = Configuration.Telegram_TOKEN;
  docConfig["Topic_DHT11_Temperature"] = Configuration.Topic_DHT11_Temperature;
  docConfig["Topic_DHT11_Humidity"] = Configuration.Topic_DHT11_Humidity;
  docConfig["Topic_SoilMoisture"] = Configuration.Topic_SoilMoisture;
  File f = SPIFFS.open(fileconfig, "w");
  if (!f)
  {
    Serial.println("Fichier Config absent - Création du fichier");
  }
  serializeJson(docConfig, jsondoc);
  f.print(jsondoc); //save in spiffs
  f.close();
  Serial.println(jsondoc);
  bot = new UniversalTelegramBot(Configuration.Telegram_TOKEN, client);
  playSuccess();
}

void initWiFi()
{
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_G, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Ecran non branché");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.clearDisplay();
  display.setCursor(10, 0);
  display.println("Info de connexion");
  display.setCursor(0, 25);
  display.println("SSID : OpenMQTT");
  display.display();

  WiFi.setHostname(Configuration.hostname.c_str()); //define hostname
  wifiManager.autoConnect(ssid, password);

  display.clearDisplay();
  display.setCursor(40, 0);
  display.println("IPv6 info");
  display.setCursor(30, 25);
  display.println(WiFi.localIP());
  display.setCursor(15, 45);
  display.println(WiFi.macAddress());
  display.display();

  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.begin();

  digitalWrite(LED_G, HIGH);
}

void initSensor()
{
  if (Configuration.Topic_DHT11_Temperature != "" || Configuration.Topic_DHT11_Humidity != "")
  {
    dht.begin();
  }
}

void LoopSensor()
{
  if (Configuration.Topic_DHT11_Temperature != "")
  {
    float readTemp = dht.readTemperature();
    if (!(isnan(readTemp)))
    {
      DHT_Te = readTemp;
    }
    sprintf(data, "%f", DHT_Te);
    mqttClient.publish(Configuration.Topic_DHT11_Temperature.c_str(), data);
  }
  if (Configuration.Topic_DHT11_Humidity != "")
  {
    float readHumi = dht.readHumidity();
    if (!(isnan(readHumi)))
    {
      DHT_Hu = readHumi;
    }
    sprintf(data, "%f", DHT_Hu);
    mqttClient.publish(Configuration.Topic_DHT11_Humidity.c_str(), data);
  }

  if (Configuration.Topic_SoilMoisture != "")
  {
    SoilMoisture_Value=map(0.95*SoilMoisture_Value+0.05*analogRead(32), 0, 4096, 100, 0);//exponential smoothing of soil moisture
    sprintf(data, "%f", SoilMoisture_Value);
    mqttClient.publish(Configuration.Topic_SoilMoisture.c_str(), data);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.print("\n");

  SPIFFS.begin();

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  if (!SPIFFS.begin())
  {
    Serial.println("Erreur SPIFFS...");
    return;
  }

  while (file)
  {
    Serial.print("Fichier : ");
    Serial.println(file.name());
    file.close();
    file = root.openNextFile();
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Ecran non branché");
    playFailed();
  }

  Chargement();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html"); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/favicon.ico", "image/ico"); });

  server.on("/js/scheduler.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/js/scheduler.js", "text/javascript"); });

  server.on("/js/jquery-3.5.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/js/jquery-3.5.1.min.js", "text/javascript"); });

  server.on("/css/doc.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/css/doc.css", "text/css"); });

  server.on("/css/scheduler.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/css/scheduler.css", "text/css"); });

  server.on("/config/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/config/config.json", "application/json"); });

  server.on("/Temps", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", TempsActuel); });
            

  server.on("/InternalTemp", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Internal Temperature : " + String((temprature_sens_read() - 32) / 1.8) + " °C"); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam(INPUT_mqttip))
              {
                Configuration.mqttServer = String(request->getParam(INPUT_mqttip)->value());
              }
              else if (request->hasParam(INPUT_hostname))
              {
                Configuration.hostname = String(request->getParam(INPUT_hostname)->value());
              }
              else if (request->hasParam(INPUT_telegramID1))
              {
                Configuration.Telegram_ID_1 = request->getParam(INPUT_telegramID1)->value();
              }
              else if (request->hasParam(INPUT_telegramID2))
              {
                Configuration.Telegram_ID_2 = request->getParam(INPUT_telegramID2)->value();
              }
              else if (request->hasParam(INPUT_telegramID3))
              {
                Configuration.Telegram_ID_3 = request->getParam(INPUT_telegramID3)->value();
              }
              else if (request->hasParam(INPUT_telegramID4))
              {
                Configuration.Telegram_ID_4 = request->getParam(INPUT_telegramID4)->value();
              }
              else if (request->hasParam(INPUT_telegramID5))
              {
                Configuration.Telegram_ID_5 = request->getParam(INPUT_telegramID5)->value();
              }
              else if (request->hasParam(INPUT_telegramToken))
              {
                Configuration.Telegram_TOKEN = request->getParam(INPUT_telegramToken)->value();
              }
              else if (request->hasParam(INPUT_Topic_DHT11_Temperature))
              {
                Configuration.Topic_DHT11_Temperature = request->getParam(INPUT_Topic_DHT11_Temperature)->value();
              }
              else if (request->hasParam(INPUT_Topic_DHT11_Humidity))
              {
                Configuration.Topic_DHT11_Humidity = request->getParam(INPUT_Topic_DHT11_Humidity)->value();
              }
              else if (request->hasParam(INPUT_Topic_SoilMoisture))
              {
                Configuration.Topic_SoilMoisture = request->getParam(INPUT_Topic_SoilMoisture)->value();
              }
              else
              {
                Serial.println("/get failed");
                playFailed();
              }
              Sauvegarde();
              request->send(204);
            });

  initWiFi();
  initSensor();

  setupMQTT();

  AsyncElegantOTA.begin(&server);
}

void loop()
{
  ActualisationTempsServeur();

  AsyncElegantOTA.loop();
  if (Configuration.mqttServer != "")
  {
    mqttClient.loop();
    if (!mqttClient.connected())
    {
      reconnect();
    }
  }

  if (resultHeure == 0 && resultMinute == 0 && resultSeconde == 0)
  {
    ESP.restart();
  }

  if (millis() > IntervalRoutine + TestRoutinePrecedente)
  {
    int NombreMessagesRecu = bot->getUpdates(bot->last_message_received + 1);
    while (NombreMessagesRecu)
    {
      Message_Recu(NombreMessagesRecu);
      NombreMessagesRecu = bot->getUpdates(bot->last_message_received + 1);
    }
    LoopSensor();
    TestRoutinePrecedente = millis();
  }

  delay(100);
}