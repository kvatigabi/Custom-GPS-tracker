#include <BufferedPrint.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <RingBuf.h>
#include <SdFat.h>
#include <SdFatConfig.h>
#include <sdios.h>

#include <LiquidCrystal_I2C.h>
#include <NMEAGPS.h>



#define LED_B PB4
#define LED_R PB3
#define BUTTON1 PA4
#define BUTTON2 PA5
bool isSdInitfailure = false;
char DataFileName[15];
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
HardwareSerial Serial3(PB0, PB2);

NMEAGPS gps;

gps_fix fix;
SdFs sd;
FsFile DataFile;

void setup() {
  // put your setup code here, to run once:
#define CS_PIN PA8
  pinMode(CS_PIN, OUTPUT);
  pinMode(BUTTON1, INPUT);
  pinMode(BUTTON2, INPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_B, OUTPUT);
  Serial.setRx(PA10);
  Serial.setTx(PA9);
  pinMode(PB6, OUTPUT_OPEN_DRAIN);
  pinMode(PB7, OUTPUT_OPEN_DRAIN);
  //




  //  lcd.init();
  //  lcd.backlight();
  Serial3.begin(9600);
  //Serial.begin(9600);

  if (!sd.begin(CS_PIN, SPI_HALF_SPEED)) {
    //Serial.println(F("SD ERR"));
    isSdInitfailure = true;
    while (isSdInitfailure) {
      digitalWrite(LED_B, HIGH);
      digitalWrite(LED_R, HIGH);
      if(sd.begin(CS_PIN, SPI_HALF_SPEED)){
        isSdInitfailure = false;
        
      }
      delay(2000);
      //Serial.println("RESTART ATTEMPT");
      digitalWrite(LED_B, LOW);
      digitalWrite(LED_R, LOW);
    }
  }
  //Serial.println("SD GOOD");

  //    SPI.setClockDivider(SPI_CLOCK_DIV2);

}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMillis = millis();
  static unsigned long previousMillis = 0;
  static unsigned long sdPreviousMillis = 0;
  static bool ledState = false;
  static unsigned long sdCheckInterval = 60000; //one minute sd check interval

  while (gps.available(Serial3)) {
    fix = gps.read();
    CreateFilename(DataFileName);
    SDwrite();

    if (fix.valid.location) {
      if (currentMillis - previousMillis >= 500) {
        previousMillis = currentMillis;

        if (ledState) {
          digitalWrite(LED_B, LOW);
          ledState = false;
        }
        else {
          digitalWrite(LED_B, HIGH);
          ledState = true;
        }
      }
    }
  }
  if (currentMillis - sdPreviousMillis >= sdCheckInterval) { //sd check timer
    sdPreviousMillis = currentMillis;
    isSdStillConnected(currentMillis);
    //Serial.println("CHECK GOOD");
  }
Button1Check(currentMillis);
}
void GetPrintTime() {
  if (fix.valid.time) {
    lcd.setCursor(8, 1);
    lcd.print(fix.dateTime.hours);
    lcd.print(":");
    lcd.print(fix.dateTime.minutes);
  }
}
void GetPrintPdop() {
  if (fix.valid.pdop) {
    lcd.setCursor(12, 0);
    lcd.print(fix.pdop);
    lcd.setCursor(0, 1);
    lcd.print(fix.satellites);
  }
}
void GetPrintKmh() {
  if (fix.valid.speed) {
    lcd.setCursor(0, 0);
    lcd.print(round(fix.speed_kph()));
    lcd.setCursor(7, 0);
    lcd.print("Kmh");
  }
}
void sdErrStatusLed(unsigned long currentMillis) {
  static unsigned long previousMillis = 0;
  static int ledBlinkIntervalMs = 500;
  static bool ledState = false;
  if (!isSdInitfailure) {
    if (currentMillis - previousMillis >= ledBlinkIntervalMs) {
      previousMillis = currentMillis;

      if (ledState) {
        digitalWrite(LED_R, LOW);
        ledState = false;
      }
      else {
        digitalWrite(LED_R, HIGH);
        ledState = true;
      }
    }
  }
}
void SDwrite() {
  if (fix.valid.location) {
    DataFile.print(fix.dateTime.hours);
    DataFile.print(F(":"));
    DataFile.print(fix.dateTime.minutes);
    DataFile.print(F(":"));
    DataFile.print(fix.dateTime.seconds);
    DataFile.print(F(","));
    DataFile.print(fix.latitudeL());
    DataFile.print(F(","));
    DataFile.print(fix.longitudeL());
    DataFile.print(F(","));
    DataFile.println(fix.altitude());
    DataFile.flush();
    //Serial.println("data written");
    //TODO: check if fix.altitude is valid and then write it else will print 0
  }
}
void CreateFilename(char DataFileName[15]) {
  static bool isFileCreated = false;

  if (fix.valid.date && !isFileCreated)
    sprintf(DataFileName, "%d-%d-%d.csv", fix.dateTime.date, fix.dateTime.month, fix.dateTime.year);
  if (!sd.exists(DataFileName)) {

    DataFile = sd.open(DataFileName, FILE_WRITE);//create/open it
    if (DataFile) { //if it opened corerectly write this below
      DataFile.print(F("Time"));
      DataFile.print(F(","));
      DataFile.print(F("Lat"));
      DataFile.print(F(","));
      DataFile.print(F("Long"));
      DataFile.print(F(","));
      DataFile.println(F("Alt(M)"));
      DataFile.flush();
      isFileCreated = true;
      //Serial.println("file created!");
      //    Serial.println("FileName opened and written");
    }
  }
  else {
    //    Serial.println(F("could not create/open DataFile"));
  }
}

bool isSdStillConnected(unsigned long currentMillis) { //check if sd is connected. If not retry every sdRestartInterval
  static long prevMillis = 0;
  static long sdTimPrevMillis = 0;
  static int ledBlinkIntervalMs = 100;
  static int sdRestartInterval = 1000;
  static bool ledState = false;
  DataFile.close();
  sd.end();

  if (!sd.begin(CS_PIN, SPI_HALF_SPEED)) {
    while (true) {
      digitalWrite(LED_B, LOW);
      currentMillis = millis();
      if (currentMillis - prevMillis >= ledBlinkIntervalMs) {
        prevMillis = currentMillis;

        if (ledState) {
          digitalWrite(LED_R, LOW);
          ledState = false;
        }
        else {
          digitalWrite(LED_R, HIGH);
          ledState = true;
        }
        if (currentMillis - sdTimPrevMillis >= sdRestartInterval) {
          sdTimPrevMillis = currentMillis;
          if (sd.begin(CS_PIN, SPI_HALF_SPEED)) {
            DataFile = sd.open(DataFileName, FILE_WRITE);
            Serial.println("SD OPEN SUCCESS");
            digitalWrite(LED_R, LOW); //led might stay on after if else loop. close it!
            break;
            return true;
          }
        }
      }
    }
  }
  DataFile = sd.open(DataFileName, FILE_WRITE);
  //Serial.println("check func passed straight away");
  digitalWrite(LED_R, LOW); //led might stay on after if else loop. close it!
  return true;
}



void Button1Check(unsigned long currentMillis) {
  static unsigned long buttonPrevMillis = 0;
  static int ledBlinkIntervalMs = 100;
  static long sdTimPrevMillis = 0;
  static int sdRestartInterval = 1000;
  static bool ledState = false;

  if (digitalRead(BUTTON1) && currentMillis - buttonPrevMillis >= 2000) { //manual SD ground check
    buttonPrevMillis = currentMillis;
    DataFile.close();
    sd.end();
   
    if (!sd.begin(CS_PIN, SPI_HALF_SPEED)) {
      digitalWrite(LED_B, LOW);
      digitalWrite(LED_R, LOW);

      
      while (true) {
        currentMillis = millis();
        if (currentMillis - buttonPrevMillis >= ledBlinkIntervalMs) {
          buttonPrevMillis = currentMillis;

          if (ledState) {
            digitalWrite(LED_R, LOW);
            digitalWrite(LED_B, HIGH);
            ledState = false;
          }
          else {
            digitalWrite(LED_R, HIGH);
            digitalWrite(LED_B, LOW);
            ledState = true;
          }
          if (currentMillis - sdTimPrevMillis >= sdRestartInterval) {
            sdTimPrevMillis = currentMillis;
            if (sd.begin(CS_PIN, SPI_HALF_SPEED)) {
              DataFile = sd.open(DataFileName, FILE_WRITE);
              //Serial.println("SD BUTTON CHECK OPEN SUCCESS");
              digitalWrite(LED_R, LOW); //led might stay on after if else loop. close it!
              break;
              
            }
          }
        }
      }
    }
    DataFile = sd.open(DataFileName, FILE_WRITE);
    //Serial.println("BUTTONcheck func passed straight away");
    while(digitalRead(BUTTON1)){
      digitalWrite(LED_B, LOW);
     digitalWrite(LED_R, HIGH); //to show us check passed successfully
    }
    digitalWrite(LED_R, LOW); //led might stay on after if else loop. close it!
  }
}
