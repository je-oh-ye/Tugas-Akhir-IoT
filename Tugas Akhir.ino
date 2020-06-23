#include <WiFi.h>
#include <MQTT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HCSR04.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

UltraSonicDistanceSensor distanceSensor(32, 33); //initialisation class HCSR04 (trig pin , echo pin)

//Timer set
unsigned long previousMillis = 0;    // Stores last time temperature was published
const long interval = 5000;

const char ssid[] = "BERSINAR";
const char pass[] = "tanyajeki";

const int RST_PIN = 22;
const int SS_PIN = 21;
const int relayPin = 15;

String pesan = "Access Denied";
String kartu = "";

unsigned long now = millis();
unsigned long lastTrigger = 0;
boolean startTimer = false;

int oldDistance;
int newDistance;

int stateMotion = LOW;             // default tidak ada gerakan
int valMotion = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

WiFiClient net;
MQTTClient client;

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("Jolevin", "Jolevin01", "71170180")) { //client_id, username, password
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
  if (kartu == "Access Granted") {
    if (topic == "btnRelay" && payload == "1") {
      client.publish("relay", "Terbuka");
      digitalWrite(relayPin, HIGH);
    }else if (topic == "sensor" && payload == "Jarak Benar") {
      client.publish("relay", "Terbuka");
      digitalWrite(relayPin, HIGH);
      delay(3000);
    } else {
      client.publish("relay", "Tertutup");
      digitalWrite(relayPin, LOW);
    }
  }
}

void setup() {
  Serial.begin(115200); // Initialize serial communications with the PC
  WiFi.begin(ssid, pass);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  oldDistance = distanceSensor.measureDistanceCm();

  while (!Serial); // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details

  client.begin("broker.shiftr.io", net);
  client.onMessage(messageReceived);
  connect();
  client.subscribe("btnRelay");
  client.subscribe("sensor");
}

void loop() {
  client.loop();
  delay(500);

  if (!client.connected()) {
    connect();
  }

  newDistance = distanceSensor.measureDistanceCm();
  Serial.println(newDistance);
  if (newDistance != oldDistance && newDistance <= 50) {
    client.publish("sensor", "Jarak Benar");
    oldDistance = newDistance;
  } else {
    client.publish("sensor", "Jarak Salah");
    oldDistance = newDistance;
  }

  //RFID
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  String uid;
  String temp;
  for (int i = 0; i < 4; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      temp = "0" + String(mfrc522.uid.uidByte[i], HEX);
    }
    else temp = String(mfrc522.uid.uidByte[i], HEX);

    if (i == 3) {
      uid =  uid + temp;
    }
    else uid =  uid + temp + " ";
  }
  Serial.println("UID " + uid);
  String grantedAccess = "0a 19 86 19"; //Akses RFID yang ditunjuk
  grantedAccess.toLowerCase();

  //Pengiriman data RFID via Bluetooth
  if (uid == grantedAccess) {
    Serial.println("Access Granted");
    client.publish("rfid", "Access Granted");
    kartu = "Access Granted";
  }
  else {
    Serial.println("Access Denied");
    client.publish("rfid", "Access Denied");
  }
  Serial.println("\n");
  mfrc522.PICC_HaltA();
  delay(3000);
}
