#include <HomeSpan.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <ESPmDNS.h>

const char* ssid        = "My Wi-Fi SSID";      // Wi-Fi SSID
const char* password    = "My Wi-Fi Password";  // Wi-Fi Password
const char* pairingCode = "12341234";           // Pairing Code. Not Allowed: 00000000, 11111111, 22222222, 33333333, 44444444, 55555555, 66666666, 77777777, 88888888, 99999999, 12345678, 87654321
String serialNumber     = "70886B......";       // Sensor Serial Number
String hostnamePrefix   = "AWAIR-ELEM";

SpanCharacteristic *airQuality, *vocDensity, *pm25Density, *carbonDioxideDetected, *carbonDioxideLevel, *currentRelativeHumidity, *currentTemperature;
String deviceNumber = serialNumber.substring(serialNumber.length() - 6);
String hostname = hostnamePrefix + "-" + deviceNumber;
String apiURL;
long long lastUpdateTime;
bool mdnsResolved = false;

void setup() {
  Serial.begin(115200);
  homeSpan.setWifiCredentials(ssid, password);
  homeSpan.setPairingCode(pairingCode);
  homeSpan.begin(Category::Sensors, "Awair Sensor");

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();          
      new Characteristic::Manufacturer("Awair");
      new Characteristic::SerialNumber(serialNumber.c_str());

    new Service::AirQualitySensor();
      airQuality = new Characteristic::AirQuality();
      vocDensity = new Characteristic::VOCDensity();
      pm25Density = new Characteristic::PM25Density();

    new Service::CarbonDioxideSensor();
      carbonDioxideDetected = new Characteristic::CarbonDioxideDetected();
      carbonDioxideLevel = new Characteristic::CarbonDioxideLevel();

    new Service::HumiditySensor();
      currentRelativeHumidity = new Characteristic::CurrentRelativeHumidity();

    new Service::TemperatureSensor();
      currentTemperature = new Characteristic::CurrentTemperature();
}

void loop() {
  if (millis() > lastUpdateTime + 10000) {
    if (mdnsResolved) {
      lastUpdateTime = millis();

      String response = httpGETRequest((apiURL + "/air-data/latest").c_str());
      JSONVar decodedResponse = JSON.parse(response);

      double score = decodedResponse["score"];
      airQuality->setVal(round(5 - score / 100 * 4));
      vocDensity->setVal((double)decodedResponse["voc"]);
      pm25Density->setVal((double)decodedResponse["pm25"]);
      double co2 = decodedResponse["co2"];
      carbonDioxideLevel->setVal(co2);
      carbonDioxideDetected->setVal(co2 >= 800 ? 1 : 0);
      currentRelativeHumidity->setVal((double)decodedResponse["humid"]);
      currentTemperature->setVal((double)decodedResponse["temp"]);
    } else {
      apiURL = resolveMdnsService(hostname.c_str());
      if (apiURL != "")
        mdnsResolved = true;
    }
  }

  homeSpan.poll();
}

String resolveMdnsService(const char* hostname) {
  if (mdns_init() != ESP_OK) {
    Serial.println("mDNS failed to start");
    return "";
  }
 
  int servicesCount = MDNS.queryService("http", "tcp");
   
  if (servicesCount == 0) {
    Serial.println("No services were found.");
    return "";
  } else {
    for (int i = 0; i < servicesCount; i++) {
      if(strcmp(MDNS.hostname(i).c_str(), hostname) == 0) {
        return "http://" + MDNS.IP(i).toString() + ":" + MDNS.port(i);
      }
    }
  }
}

String httpGETRequest(const char* url) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);

  int httpResponseCode = http.GET();
  String response = "{}"; 
  if (httpResponseCode > 0)
    response = http.getString();
  Serial.println(response);

  http.end();
  return response;
}