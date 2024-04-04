// Energy Meter
// This code is for This code is for NodeMCU v0.9 (ESP8266) based Energy monitoring Device
// This code is a modified version of sample code from https://github.com/pieman64/ESPecoMon
// 

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
//#define BLYNK_DEBUG
#define BLYNK_PRINT Serial
#include <ArduinoOTA.h>
//#include <ESP8266WiFi.h>
//#include <BlynkSimpleEsp8266.h>
#define CLOUD  // comment out for local server

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
//#define OLED_RESET 0  // GPIO0
//Adafruit_SSD1306 display(OLED_RESET);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

 // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, 0); // -1

// File system object
File myFile;

RTC_DS1307 rtc;

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
float Wh = 0 ;           // Energy in Wh
float kWh = 0;           // Energy in kWh
unsigned long last_time =0;
unsigned long current_time =0;
unsigned long interval = 100;
unsigned int calibration = 100;  // V2 slider calibrates this // 100
unsigned int pF = 95;           // Power Factor default 95 / 85
float bill_amount = 0;   // 30 day cost as present energy usage incl approx PF 
unsigned int energyTariff = 0.20; // Energy cost in INR per unit (kWh)

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = SS;

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
  Vrms = (Vpp/2.0) *0.707; 
  Vrms = Vrms - (calibration / 10000.0);     // calibtrate to zero with slider
  // FIX ADDED
  if(Vrms < 0.0) {
    Vrms = 0.0;
  }
  Serial.println(Vrms);
  Irms = (Vrms * 1000)/Sensitivity;
  if((Irms > -0.015) && (Irms < 0.008)){  // remove low end chatter
    Irms = 0.0;
  }
  power= (Supply_Voltage * Irms) * (pF / 100.0); 
  last_time = current_time;
  current_time = millis();    
  Wh = Wh+  power *(( current_time -last_time) /3600000.0) ; // calculating energy in Watt-Hour
  // ADDED
  kWh = Wh / 1000;
  bill_amount = Wh * (energyTariff/1000);
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
  while((millis()-start_time) < 950) //read every 0.95 Sec
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
void displaydata() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(30, 6);
  display.println("Energy Meter"); 
  
  display.setCursor(10, 20);
  display.println("Pow  : "); 
  display.setCursor(50, 20);  
  display.println(power*0.001);
  display.setCursor(85, 20);
  display.println("kW"); 

  display.setCursor(10, 30);
  display.println("Irms "); 
  display.setCursor(50, 30);  
  display.println(Irms);
  display.setCursor(85, 30);
  display.println("A");
  
  display.setCursor(10, 40);
  display.println("Ener : ");  
  display.setCursor(50, 40);
  display.println(kWh);
  display.setCursor(85, 40);
  display.println("kWh");

  display.setCursor(10, 50);
  display.println("Bill : "); 
  display.setCursor(50, 50);
  display.println(bill_amount);
  display.setCursor(85, 50);
  display.println("EUR");
  display.display();
}


void setup() {

  // Power to OLED screeen
//  pinMode(2, OUTPUT);
//  pinMode(3, OUTPUT);
//  digitalWrite(2, LOW); // 0 V GND
//  digitalWrite(3, HIGH); // 5 V 

  // ESP8266
  //Wire.begin(D2, D1); /* join i2c bus with SDA=D2 and SCL=D1 of NodeMCU */
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //display.begin();   
  //WiFi.hostname(myhostname);
  Serial.begin(115200); 
  Serial.println("\n Rebooted");
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

  pinMode(pinCS, OUTPUT);

 // SD Card Initialization
  if (!SD.begin()) {
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
    // rtc.adjust(DateTime(2017, 1, 27, 12, 56, 0));
  }

}

//BLYNK_CONNECTED(){
//  Blynk.syncAll();  
//}

void loop() {
  // Power to OLED screeen
//  digitalWrite(2, LOW); // 0 V GND
//  digitalWrite(3, HIGH); // 5 V 

  getACS712();
  displaydata();
  //Blynk.run();
  //ArduinoOTA.handle();
  //timer.run();
  delay(1000);


  Serial.print(rtc.getTimeStr());
 
  myFile = SD.open("test.txt", FILE_WRITE);
  if (myFile) {    
    myFile.print(rtc.getTimeStr());
    //myFile.print(",");    
    //myFile.println(int(rtc.getTemp()));
    myFile.close(); // close the file
  }
  // if the file didn't open, print an error:
  else {
    Serial.println("error opening test.txt");
  }
 
}