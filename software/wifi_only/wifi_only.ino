
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_SCD30.h>
#include "DHT.h"

// WiFi AP SSID
#define WIFI_SSID "IDreamer"
// WiFi password
#define WIFI_PASSWORD "7758A521@"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com/"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "O0FsslHC4N1voVr4Mpf4TCG-JYC0KMWxiQvZoxzh0hVAx8vLjMKmHY92puxcxhvDR8GDW9JJCdGU1OfzfXSUuw=="
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "wanghanshuo1220@gmail.com"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "esp32"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "PST8PDT"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// ************** sensor SGP ***********************
Point sensor_SGP("SGP");
#define SDA_SGP 19
#define SCL_SGP 23
Adafruit_SGP30 sgp;
unsigned int eco2;
unsigned int tvoc;

// ************** sensor SCD ***********************
Point sensor_SCD("SCD");
// #define SDA_SCD 19
// #define SCL_SCD 18
Adafruit_SCD30  scd30;

// ************** sensor DHT ***********************
Point sensor_DHT("DHT11");
#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


// ************** Fan pin ***********************
#define Fan 4


void timeSync() {
  // Synchronize UTC time with NTP servers
  // Accurate time is necessary for certificate validaton and writing in batches
  configTime(0, 0, "pool.ntp.org", "time.nis.gov");
  // Set timezone
  setenv("TZ", TZ_INFO, 1);

  // Wait till time is synced
  Serial.print("Syncing time");
  int i = 0;
  while (time(nullptr) < 1000000000ul && i < 100) {
    Serial.print(".");
    delay(100);
    i++;
  }
  Serial.println();

  // Show time
  time_t tnow = time(nullptr);
  Serial.print("Synchronized time: ");
  Serial.println(String(ctime(&tnow)));
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire1.begin(SDA_SGP, SCL_SGP);

  // setup fan
  pinMode(Fan, OUTPUT);

  // setup dht
  dht.begin();

  // setup scd
  Serial.println("finding SCD30 chip");
  while (!scd30.begin()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("SCD30 Found!");


  // setup sgp
  Serial.println("Finding SGP30 chip");
  while (!sgp.begin(&Wire1, true)) {
    Serial.print(".");
    delay(100);
  }

  sgp.setHumidity(8200);
  
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // Setup wifi
  Serial.print("Connecting to wifi");

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Add tags
  sensor_SGP.addTag("deviceSGP", DEVICE);
  sensor_DHT.addTag("deviceDHT", DEVICE);
  sensor_SCD.addTag("deviceSCD", DEVICE);

  // Sync time for certificate validation
  timeSync();

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

    // the first eco2 meaurements are always 400
    do {
        while (!sgp.IAQmeasure()) {
            Serial.print(".");
            delay(100);
        }
        tvoc = sgp.TVOC;
        Serial.print("TVOC(ppb):");
        Serial.print(tvoc);

        eco2 = sgp.eCO2;
        tvoc = sgp.TVOC;
        Serial.print(" eco2(ppm):");
        Serial.println(eco2);
        delay(1000);
    } while (eco2 == 400);

    // entryLoop = millis();
}

void loop() {

    // ***************** get SGP data ********************
    Serial.println(" looping !");
    Serial.println("");
    while (!sgp.IAQmeasure()) {
        Serial.print(".");
        delay(100);
    }
    Serial.println("******** SGP ********");
    tvoc = ((19 * tvoc) + sgp.TVOC) / 20;
    Serial.print("TVOC(ppb):");
    Serial.print(tvoc);

    eco2 = ((19 * eco2) + sgp.eCO2) / 20;
    Serial.print(" eco2(ppm):");
    Serial.print(eco2);

    sensor_SGP.clearFields();
    float TVOC = tvoc;
    float eCO2 = eco2;
    Serial.println("TVOC: "+String(TVOC)+"eCO2 "+String(eCO2));

    sensor_SGP.addField("TVOC", TVOC);
    sensor_SGP.addField("eCO2", eCO2);

    // ***************** get SCD data ********************
    Serial.println("******** SCD ********");
    static float co2;
    if (scd30.dataReady()){
      Serial.println("Data available!");

      if (!scd30.read()){ Serial.println("Error reading sensor data"); return; }

      Serial.print("Temperature: ");
      Serial.print(scd30.temperature);
      Serial.println(" degrees C");
      
      Serial.print("Relative Humidity: ");
      Serial.print(scd30.relative_humidity);
      Serial.println(" %");
      
      Serial.print("CO2: ");
      Serial.print(scd30.CO2, 3);
      co2 = scd30.CO2;
      Serial.println(" ppm");
      Serial.println("");
    }
    sensor_SCD.clearFields();
    // scd30.read();
    
    sensor_SCD.addField("CO2", co2);


    // run fan
    if (co2 >= 1400){
      digitalWrite(Fan, 255);
    }else{
      digitalWrite(Fan, 0);
    }

    // ***************** get DHT data ********************
    Serial.println("******** DHT ********");
    sensor_DHT.clearFields();
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    Serial.println("hum: "+String(hum)+"temp "+String(temp));

    sensor_DHT.addField("hum", hum);
    sensor_DHT.addField("temp", temp);


    // If no Wifi signal, try to reconnect it
    if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED))
        Serial.println("Wifi connection lost");
    // Write point
    if (!client.writePoint(sensor_SGP)) {
        Serial.print("InfluxDB write SGD failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    if (!client.writePoint(sensor_DHT)) {
        Serial.print("InfluxDB write DHT failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    if (!client.writePoint(sensor_SCD)) {
        Serial.print("InfluxDB write SCD failed: ");
        Serial.println(client.getLastErrorMessage());
    }

    //Wait 10s
    Serial.println("Wait 1s");
    delay(1000);
}
