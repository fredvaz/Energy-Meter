// Energy Meter
// This code is for This code is for NodeMCU v0.9 (ESP8266) based Energy monitoring Device
// This code is a modified version of sample code from https://github.com/pieman64/ESPecoMon
//

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include "DHT.h"
//#define BLYNK_DEBUG
#define BLYNK_PRINT Serial
#include <ArduinoOTA.h>
//#include <ESP8266WiFi.h>
//#include <BlynkSimpleEsp8266.h>
#define CLOUD  // comment out for local server

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

//#define OLED_RESET 0  // GPIO0
//Adafruit_SSD1306 display(OLED_RESET);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Uncomment one of the lines below for whatever DHT sensor type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTPIN 2 // D3

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, 0); // -1

// define device I2C address: 0x76 or 0x77 (0x77 is library default address)
#define BMP280_I2C_ADDRESS  0x76
// initialize Adafruit BMP280 library
Adafruit_BMP280 bmp280;

// File system object
File myFile;

RTC_DS1307 rtc;

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


//BlynkTimer timer;

char auth[]       = "xxxx";
char ssid[]       = "xxxx";
char pass[]       = "xxxx";
char server[]     = "blynk-cloud.com";    // ip or domain
char myhostname[] = "Energy-Meter-V2.0";  // for OTA and router identification

const int Sensor_Pin = A0;
unsigned int Sensitivity = 66;   // 185mV/A for 5A, 100 mV/A for 20A and 66mV/A for 30A Module
float Vpp = 0; // peak-peak voltage
float Vrms = 0; // rms voltage
float Irms = 0; // rms current
float Supply_Voltage = 233.0;           // Measured from Multimeter wilt a Bulb 23W
float Vcc = 3.3;         // Arduino Mega ADC ref 5V pin // NodeMCU v0.9 ESP8266 ADC ref 3.3V
float power = 0;         // power in watt
float Wh = 00000.0 ;           // Energy in Wh
float kWh = 0.0;           // Energy in kWh // 17.71
unsigned long last_time = 0;
unsigned long current_time = 0;
unsigned long interval = 100;
unsigned int calibration = 100;  // V2 slider calibrates this // 100
unsigned int pF = 95;           // Power Factor default 95 / 85
float bill_amount = 0;   // 30 day cost as present energy usage incl approx PF
float energyTariff = 0.20; // Energy cost in INR per unit (kWh) // FIXED
float temp = 0.0; // Temperature
float temp_offset = 2.75; // Offset temperature
float hic = 0.0;
float humidity = 0.0;
float pressure = 0.0; // Pressure

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t pinCS  = 15;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "Data"
//------------------------------------------------------------------------------

// Time in micros for next data record.
uint32_t logTime;


void getACS712() {  // for AC
  Vpp = getVPP();
  Vrms = (Vpp / 2.0) * 0.707;
  Vrms = Vrms - (calibration / 10000.0);     // calibtrate to zero with slider
  // FIX ADDED
  if (Vrms < 0.0) {
    Vrms = 0.0;
  }
  //Serial.println(Vrms);
  Irms = (Vrms * 1000) / Sensitivity;
  if ((Irms > -0.015) && (Irms < 0.008)) { // remove low end chatter -0.015 & 0.008 // FIXED
    Irms = 0.0;
  }
  power = (Supply_Voltage * Irms) * (pF / 100.0);
  last_time = current_time;
  current_time = millis();
  Wh = Wh +  power * (( current_time - last_time) / 3600000.0) ; // calculating energy in Watt-Hour
  // ADDED
  kWh = Wh / 1000;
  bill_amount = kWh * energyTariff; // FIXED
  Serial.print("Irms:  ");
  Serial.print(String(Irms, 3));
  Serial.println(" A");
  Serial.print("Power: ");
  Serial.print(String(power, 3));
  Serial.println(" W");
  //  Serial.print("Bill Amount: INR");
  //  Serial.println(String(bill_amount, 2));
  //  Blynk.virtualWrite(V0, String (Wh));  // gauge
  //  Blynk.virtualWrite(V1, String(bill_amount, 2));
  //  Blynk.virtualWrite(V2, String(power,2));
  //  Blynk.virtualWrite(V3, String(Irms, 3));
}

float getVPP() {

  float result;
  int readValue;
  int maxValue = 0;
  int minValue = 1024;
  uint32_t start_time = millis();
  while ((millis() - start_time) < 950) //read every 0.95 Sec
  {
    readValue = analogRead(Sensor_Pin);
    //Serial.println(readValue);
    if (readValue > maxValue)
    {
      maxValue = readValue;
    }
    if (readValue < minValue)
    {
      minValue = readValue;
    }
  }
  result = ((maxValue - minValue) * Vcc) / 1024.0;
  return result;
}

//BLYNK_WRITE(V4) {  // calibration slider 50 to 200
//    calibration = param.asInt();
//    }
//BLYNK_WRITE(V5) {  // set supply voltage slider 70 to 260
//    Supply_Voltage = param.asInt();
//    }
//BLYNK_WRITE(V6) {  // PF slider 60 to 100 i.e 0.60 to 1.00, default 85
//   pF = param.asInt();
//   }
//BLYNK_WRITE(V7) {  // Energy tariff slider 1 to 20, default 8 (Rs.8.0 / kWh)
//    energyTariff = param.asInt();
//}
//BLYNK_WRITE(V8) {  // set 5, 20 or 30A ACS712 sensor with menu
//    switch (param.asInt())
//    {
//      case 1: {       // 5A
//      Sensitivity = 185;
//        break;
//      }
//      case 2: {       // 20A
//       Sensitivity  = 100;
//        break;
//      }
//      case 3: {       // 30A
//       Sensitivity  = 66;
//        break;
//      }
//      default: {      // 5A
//      Sensitivity  = 185;
//      }
//    }
//}

void getTemp() {
  // TO ADD AN AVERAGE
  // read temperature and pressure from the BMP280 sensor
  //  temp     = bmp280.readTemperature() - temp_offset;   // get temperature // 100 Pa = 1 millibar (Pa = newton per square meter)
  //  pressure = bmp280.readPressure() / 100;              // get pressure
  // Wait a few seconds between measurements.
  delay(2000);
  temp = dht.readTemperature(); // Gets the values of the temperature
  humidity = dht.readHumidity(); // Gets the values of the humidity
  // Compute heat index in Celsius (isFahreheit = false)
  hic = dht.computeHeatIndex(temp, humidity, false);

  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" ºC");
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("Apparent = ");
  Serial.print(hic);
  Serial.println(" ºC");
  //  Serial.print("Pressure = ");
  //  Serial.print(pressure);
  //  Serial.println(" mb");
  //Serial.print("Approx Altitude = ");
  //Serial.print(bmp280.readAltitude(1013.25)); // This should be lined up (atmospheric pressure at sea level is 1013 millibars)
  //Serial.println(" m");
}

void displaydata() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(10, 6);
  display.println("Energy Meter");
  display.setCursor(85, 6);
  if (power > 1500) {
    display.println("ON");
  }else{
    display.println("OFF");
  }

  display.setCursor(10, 20);
  //display.println("Pow  : ");
  display.println("Ener : ");
  display.setCursor(50, 20);
  //display.println(power * 0.001);
  display.println(kWh);
  display.setCursor(85, 20);
  //display.println("kW");
  display.println("kWh");

  display.setCursor(10, 30);
  //display.println("Irms :");
  display.println("Temp : ");
  display.setCursor(50, 30);
  //display.println(Irms);
  display.println(temp);
  display.setCursor(85, 30);
  //display.println("A");
  display.print((char)247);
  display.println("C");

  display.setCursor(10, 40);
  //display.println("Ener : ");
  display.println("Humi : ");
  display.setCursor(50, 40);
  //display.println(kWh);
  display.println(humidity);
  display.setCursor(85, 40);
  //display.println("kWh");
  display.println("%");

  display.setCursor(10, 50);
  //display.println("Bill : ");
  display.println("Appa : ");
  display.setCursor(50, 50);
  //display.println(bill_amount);
  display.println(hic);
  display.setCursor(85, 50);
  //display.println("EUR");
  display.print((char)247);
  display.println("C");

  display.display();
}

void writeData() {

  DateTime now = rtc.now();

  myFile = SD.open("test.txt", FILE_WRITE);

  if (myFile) {

    myFile.print(now.day(), DEC);
    myFile.print('/');
    myFile.print(now.month(), DEC);
    myFile.print('/');
    myFile.print(now.year(), DEC);
    myFile.print(",");
    myFile.print(now.hour(), DEC);
    myFile.print(':');
    myFile.print(now.minute(), DEC);
    myFile.print(':');
    myFile.print(now.second(), DEC);
    myFile.print(",");
    myFile.print(kWh);
    myFile.print(",");
    myFile.print(power);
    myFile.print(",");
    myFile.print(temp);
    myFile.print(",");
    myFile.print(humidity);
    myFile.print(",");
    myFile.print(hic);
    myFile.println("");
    myFile.close(); // close the file

  }
  // if the file didn't open, print an error:
  else {
    Serial.println("error opening test.txt");
  }

  Serial.print("Date: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.println(") ");
  Serial.print("Time: ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

}

void setup() {

  //  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(pinCS, OUTPUT);
  pinMode(DHTPIN, INPUT);
  dht.begin();

  // ESP8266
  //Wire.begin(D2, D1); /* join i2c bus with SDA=D2 and SCL=D1 of NodeMCU */

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //display.begin();
  //WiFi.hostname(myhostname);
  Serial.begin(115200);
  //Serial.println("\n Rebooted");
  //WiFi.mode(WIFI_STA);
  //  #ifdef CLOUD
  //    Blynk.begin(auth, ssid, pass);
  //  #else
  //    Blynk.begin(auth, ssid, pass, server);
  //  #endif
  //  while (Blynk.connect() == false) {}
  //  ArduinoOTA.setHostname(myhostname);
  //  ArduinoOTA.begin();
  //  timer.setInterval(2000L, getACS712); // get data every 2s


  // SD Card Initialization
  if (!SD.begin(pinCS)) {
    Serial.println("SD card initialization failed");
    return;
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC lost power, lets set the time!");

    // Comment out below lines once you set the date & time.
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Following line sets the RTC with an explicit date & time
    // for example to set January 27 2017 at 12:56 you would call:
    //rtc.adjust(DateTime(2020, 12, 13, 16, 35, 40));
  }
  //rtc.adjust(DateTime(2020, 12, 13, 16, 30, 40));

  // initialize the BMP280 sensor
  //  if ( bmp280.begin(BMP280_I2C_ADDRESS) == 0 ) {
  //    // connection error or device address wrong!
  //    Serial.println("Error! No BMP Sensor Detected!!!");
  //  }


}

//BLYNK_CONNECTED(){
//  Blynk.syncAll();
//}

void loop() {

  //  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  //  // but actually the LED is on; this is because
  //  // it is active low on the ESP-01)
  //  delay(1000);                      // Wait for a second
  //  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  //  delay(1000);                      // Wait for two seconds (to demonstrate the active low LED)

  //  uint32_t start_time = millis();
  //  if((millis() - start_time) < 950) //read every 0.95 Sec

  getACS712();
  getTemp();
  displaydata();
  writeData();
  //Blynk.run();
  //ArduinoOTA.handle();
  //timer.run();
  //delay(1000);

}
