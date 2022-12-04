#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <stdlib.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Redmi Note 9"
#define WIFI_PASSWORD "987654321"

// Insert Firebase project API Key
#define API_KEY "AIzaSyDvxFhKWysBtVfpUwUw6e-ZoASMYmGmW-U"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://e-index-27552-default-rtdb.europe-west1.firebasedatabase.app/"

constexpr uint8_t RST_PIN = D3;
constexpr uint8_t SS_PIN = D4;

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

#define Tx 5
#define Rx 6

#define OK D1
#define NOTOK D2
#define SOUND D8
#define WAITING D0


String indeksID;
bool registracija = false;
int broj;
byte BlokPodataka[9];
String BrojIndeksaFromHex;
const char* HexIndeks;


void Registracija();
void IzlazakNaIspit();
void AccessGranted();
void AccessDenied();

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  // Prepare the key (used both as key A and as key B)
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  pinMode(OK, OUTPUT);
  pinMode(NOTOK, OUTPUT);
  pinMode(SOUND, OUTPUT);
}

void loop() {

  if (Firebase.RTDB.getBool(&fbdo, "Funkcionalnosti/Registracija"))
  {
    if (fbdo.to<bool>())
    {
      Registracija();
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, "Funkcionalnosti/IzlazakNaIspit"))
  {
    if (fbdo.to<bool>())
    {
      IzlazakNaIspit();
    }
  }

   if (Firebase.RTDB.getBool(&fbdo, "Funkcionalnosti/Informacije"))
  {
    if (fbdo.to<bool>())
    {
      Informacije();
    }
  }
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    indeksID += String(buffer[i], HEX);
    //HexIndeks += buffer[i];
  }
}

void Registracija() {
  String indeksBroj;
  if (Firebase.RTDB.getBool(&fbdo, "Indeksi/Broj"))
  {
    indeksBroj = fbdo.to<String>();
  }

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    indeksID = "";
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  Serial.print("UID: ");
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print("String: ");
  Serial.println(indeksID);

  if (Firebase.RTDB.getBool(&fbdo, "Student/" + indeksID))
  {
    AccessDenied();
    return;
  }

  Firebase.RTDB.setString(&fbdo, "Indeksi/ID", indeksID);
  AccessGranted();

  mfrc522.PICC_HaltA();
}

void IzlazakNaIspit()
{
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    indeksID = "";
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

  if (Firebase.RTDB.getBool(&fbdo, "Student/" + indeksID))
  {
    Serial.println();
    Serial.println("Indeks ID postoji u bazi");

    if (Firebase.RTDB.getBool(&fbdo, "Student/" + indeksID + "/PrijavaIspita"))
      if (fbdo.to<bool>()) {
        Serial.println("Imate pravo izlaska na ispit.");
        AccessGranted();
        return;
      }
      else {
        Serial.println("Nemate pravo izlaska na ispit");
        AccessDenied();
        return;
      }


  }  else {
    AccessDenied();
    Serial.println();
    Serial.println("Indeks ID ne postoji u bazi");
    return;
  }

  mfrc522.PICC_HaltA();
}

void AccessGranted()
{
  digitalWrite(OK, HIGH);
  digitalWrite(SOUND, HIGH);
  delay(100);
  digitalWrite(SOUND, LOW);
  delay(100);
  digitalWrite(SOUND, HIGH);
  delay(100);
  digitalWrite(SOUND, LOW);
  delay(700);
  digitalWrite(OK, LOW);
}

void AccessDenied()
{
  digitalWrite(NOTOK, HIGH);
  digitalWrite(SOUND, HIGH);
  delay(500);
  digitalWrite(NOTOK, LOW);
  digitalWrite(SOUND, LOW);
}

void Informacije()
{
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
    indeksID = "";
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

    if (Firebase.RTDB.getBool(&fbdo, "Student/" + indeksID + "/PrijavaIspita"))
    {
      Firebase.RTDB.setString(&fbdo, "Indeksi/ID", indeksID);
      AccessGranted();
      return;
    }
    
    AccessDenied();
}
