#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

#include <SPI.h>
#include <LoRa.h>

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

// ******* sensor SCD from farmland **********
Point sensor_SCD_farmland("SCD_farmland");


//define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

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
  while (!Serial);
  Serial.println("LoRa Receiver");

  // setup Lora
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(866E6)) {
    Serial.println(".");
    delay(500);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");


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
  sensor_SCD_farmland.addTag("deviceCD_farmland", DEVICE);

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
}

void loop() {

    // ***************** get Lora data ********************
    // try to parse packet
    Serial.print("looping !");

    static float LoRaData;
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      // received a packet
      Serial.print("Received packet '");

      // read packet
      if (LoRa.available()) {
        LoRaData = atof(LoRa.readString().c_str());
        Serial.print(LoRaData); 
      }

      // print RSSI of packet
      Serial.print("' with RSSI ");
      Serial.println(LoRa.packetRssi());
    }
    sensor_SCD_farmland.clearFields();
    sensor_SCD_farmland.addField("farmland's CO2", LoRaData);


    // If no Wifi signal, try to reconnect it
    if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED))
        Serial.println("Wifi connection lost");
    // Write point
    if (!client.writePoint(sensor_SCD_farmland)) {
        Serial.print("InfluxDB write SCD_farmland failed: ");
        Serial.println(client.getLastErrorMessage());
    }

    //Wait 10s
    Serial.println("Wait 10s");
    delay(10000);
}
