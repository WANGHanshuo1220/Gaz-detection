/*********
  Modified from the examples of the Arduino LoRa library
  More resources: https://randomnerdtutorials.com
*********/

#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_SCD30.h>
#include <Wire.h>
Adafruit_SCD30  scd30;
#define SDA_pin 21  // Define the SDA pin used for the SCD30
#define SCL_pin 22  // Define the SCL pin used for the SCD30

//define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

int counter = 0;

void setup() {
  //initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Sender");
  Serial.println("Adafruit SCD30 test!");
  //setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);
  
  //replace the LoRa.begin(---E-) argument with your location's frequency 
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  while (!LoRa.begin(866E6)) {
    Serial.println(".");
    delay(500);
  }
  Wire.begin(SDA_pin, SCL_pin);
  // Try to initialize!
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");
    
  
   // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");

  Serial.print("Measurement Interval: "); 
  Serial.print(scd30.getMeasurementInterval()); 
  Serial.println(" seconds");
  
}

void loop() {
  if (scd30.dataReady()){
    Serial.println("Data available!");
    Serial.print("Sending packet: ");
    Serial.println(counter);
    if (!scd30.read()){ Serial.println("Error reading sensor datr54ta"); return; }

  //Send LoRa packet to receiver
    LoRa.beginPacket();
    LoRa.print("Temperature: ");
    LoRa.print(scd30.temperature);
    LoRa.println(" degrees C");
    LoRa.print("Relative Humidity: ");
    LoRa.print(scd30.relative_humidity);
    LoRa.println(" %");
    LoRa.print("CO2: ");
    LoRa.print(scd30.CO2, 3);
    LoRa.println(" ppm");
    LoRa.println("");
    LoRa.endPacket();
  counter++;

  delay(10000);
}}
