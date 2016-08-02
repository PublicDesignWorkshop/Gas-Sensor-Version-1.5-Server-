/* This is for the gas sensor server */
/* Arduino UNO + Grove base shield + Sparkfun SD Shield server (server) */
/* Client is Arduino UNO + Grove base shield */
/* Sensors are spread out over 2 arduinos to limit power issues */
/* So client and server communicate sensor values over I2C */

#include <kSeries.h>
//#include <SoftwareSerial.h>
#include <Time.h>
#include <SD.h>
#include <SPI.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define LCD_ADDR    0x27  
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

#define CO2_ADDR 0x68

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message

const int chipSelect = 8;

kSeries K_30(12,13);
//SoftwareSerial mySerial(7,6);

LiquidCrystal_I2C lcd(LCD_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

void setup() {
  Wire.begin();
  
  lcd.begin(20,4,LCD_5x8DOTS);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home();
  lcd.clear();
  
  Serial.begin(9600);
//  mySerial.begin(9600);
  
  // Set SS pin as output for SD card
  pinMode(chipSelect, OUTPUT);

  // Sensors
  pinMode(A4, INPUT);
  pinMode(A3, INPUT);
  pinMode(A2, INPUT);
  pinMode(A1, INPUT);
  pinMode(A0, INPUT);

  pinMode(13, OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("init failed!");
    return;
  }
  Serial.println("init done.");
  Serial.println("Waiting for sync message");
}

void loop () {
  /* Our sensors are: */
  /* A0: MQ2 */
  /* A1: MQ3 */
  /* A2: MQ5 */
  /* A3: EtOH */
  /* I2C: CO2 */
  /* Remote sensors: */
  /* A0: MQ8 */
  /* A1: MQ9 */
  /* A2: Air quality */
  /* A3: HCHO sensor */

  char* sensorNames[] = { "MQ2", "MQ3", "MQ5", "EtOH", "CO2", "MQ8",  "MQ9", "Air", "For"};
  char dataString[134] = "";
  
  int co2Value = readCO2();
  int remoteSensors[4];
  int index = 5;
  int remoteSensorIndex = 0;

  int sensor;
  int sensors[9] = {0};
  sensors[0] = analogRead(A0);
  sensors[1] = analogRead(A1);
  sensors[2] = analogRead(A2);
  sensors[3] = analogRead(A3);
  sensors[4] = co2Value;
  
  char sensorString[22]; // = "B0999,0998,0997,0996\n";
  Wire.requestFrom(8, 21);
  while (Wire.available()) { // slave may send less than requested
    sensorString[remoteSensorIndex] = Wire.read(); // receive a byte as character
    
    remoteSensorIndex++;
  }
  Serial.println(sensorString);
//    byte size = mySerial.readBytesUntil('\n', sensorString, 21);
//    sensorString[size] = 0;
  
    // Read each sensor value
    char* sensorStart = strchr(sensorString, 'B');
    // skip past starting 'B'
    sensorStart++;
    char* sensorRead;
    while ((sensorRead = strsep(&sensorStart, ",")) != NULL) {
      sensors[index] = atoi(sensorRead);
      index++;
    }

  // we fit one sensor value in vertically on our 4x20 LCD display:
  // CO2:854|MQ2:429   E2
  // MQ3:236|MQ5:219   t8
  // MQ8:33|MQ9:339    O6
  // Air:338|For:347   H 
  // (EtOH/Alcohol sensor has a value of 286 here)
  char verticalSensor[4];
  itoa(sensors[3], verticalSensor, 10);
  
  lcd.clear();
  lcd.setCursor(0,0);
  
  // MQ2
  lcd.print(sensorNames[0]);
  lcd.print(":");
  lcd.print(sensors[0]);
  lcd.print("|");
  // MQ3
  lcd.print(sensorNames[1]);
  lcd.print(":");
  lcd.print(sensors[1]);
  
  // E(tOH)
  lcd.setCursor(18,0);
  lcd.print("E");
  lcd.print(verticalSensor[0]);
  
  lcd.setCursor(0,1);

  // MQ5
  lcd.print(sensorNames[2]);
  lcd.print(":");
  lcd.print(sensors[2]);
  lcd.print("|");
  // CO2
  lcd.print(sensorNames[4]);
  lcd.print(":");
  if(co2Value > 0) {
    lcd.print(sensors[4]);
  } else {
    lcd.print("XXX");
  }
  // Et(OH)
  lcd.setCursor(18,1);
  lcd.print("t");
  lcd.print(verticalSensor[1]);
  
  lcd.setCursor(0,2);

  // MQ8
  lcd.print(sensorNames[5]);
  lcd.print(":");
  lcd.print(sensors[5]);
  lcd.print("|");

  // MQ9
  lcd.print(sensorNames[6]);
  lcd.print(":");
  lcd.print(sensors[6]);

  // EtO(H)
  lcd.setCursor(18,2);
  lcd.print("O");
  lcd.print(verticalSensor[2]);
  
  lcd.setCursor(0,3);

  // Air
  lcd.print(sensorNames[7]);
  lcd.print(":");
  lcd.print(sensors[7]);
  lcd.print("|");

  // HCHO
  lcd.print(sensorNames[8]);
  lcd.print(":");
  lcd.print(sensors[8]);
  lcd.setCursor(18,3);
  lcd.print("H");
  lcd.print(verticalSensor[3]);
  
  if (Serial.available()) {
    processSyncMessage();
  }
  if (timeStatus()!= timeNotSet) {
    digitalClockDisplay();  
  }
  if (timeStatus() == timeSet) {
    digitalWrite(13, HIGH); // LED on if synced
    for (int analogPin = 0; analogPin < 9; analogPin++) {
      sensor = sensors[analogPin];
      if (analogPin) {
        sprintf(dataString, "%s, %s: %i", dataString, sensorNames[analogPin], sensor);
      } else {
        sprintf(dataString, "%s: %i", sensorNames[analogPin], sensor);
      }
    }
    // note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    //  bool dataFile = true;
    // if the file is available, write to it:
    if (dataFile) {
      digitalClockRecord(dataFile);
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      Serial.println(dataString);
      digitalWrite(13, HIGH);
      delay(200);
      digitalWrite(13, LOW);
      delay(200);
      digitalWrite(13, HIGH);
      delay(200);
      digitalWrite(13, LOW);
      delay(200);
    } else {
      Serial.println("error opening file");
    }
  } else {
    digitalWrite(13, LOW);  // LED off if needs refresh
  }
  
  delay(30000);
}


int readCO2() {
 int co2_value = 0;
 // We will store the CO2 value inside this variable.
 digitalWrite(13, HIGH);
 // On most Arduino platforms this pin is used as an indicator light.

 //////////////////////////
 /* Begin Write Sequence */
 //////////////////////////

 Wire.beginTransmission(CO2_ADDR);
 Wire.write(0x22);
 Wire.write(0x00);
 Wire.write(0x08);
 Wire.write(0x2A);
 Wire.endTransmission();

 /////////////////////////
 /* End Write Sequence. */
 /////////////////////////

 /*
 We wait 10ms for the sensor to process our command.
 The sensors's primary duties are to accurately
 measure CO2 values. Waiting 10ms will ensure the
 data is properly written to RAM

 */

 delay(10);

 /////////////////////////
 /* Begin Read Sequence */
 /////////////////////////

 /*
 Since we requested 2 bytes from the sensor we must
 read in 4 bytes. This includes the payload, checksum,
 and command status byte.

 */

 Wire.requestFrom(CO2_ADDR, 4);

 byte i = 0;
 byte buffer[4] = {0, 0, 0, 0};

 /*
 Wire.available() is not nessessary. Implementation is obscure but we leave
 it in here for portability and to future proof our code
 */
 while (Wire.available()) {
   buffer[i] = Wire.read();
   i++;
 }

 ///////////////////////
 /* End Read Sequence */
 ///////////////////////

 /*
 Using some bitwise manipulation we will shift our buffer
 into an integer for general consumption
 */

 co2_value = 0;
 co2_value |= buffer[1] & 0xFF;
 co2_value = co2_value << 8;
 co2_value |= buffer[2] & 0xFF;


 byte sum = 0; //Checksum Byte
 sum = buffer[0] + buffer[1] + buffer[2];
 if (sum == buffer[3]) {
   // Success!
   digitalWrite(13, LOW);
   return co2_value;
 } else {
 // Failure!
 /*
 Checksum failure can be due to a number of factors,
 fuzzy electrons, sensor busy, etc.
 */
  
   digitalWrite(13, LOW);
   return 0;
 }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.print(": "); 
}

void digitalClockRecord(File file) {
  file.print(hour());
  recordDigits(minute(), file);
  recordDigits(second(), file);
  file.print(" ");
  file.print(day());
  file.print(" ");
  file.print(month());
  file.print(" ");
  file.print(year());
  file.print(": "); 
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void recordDigits(int digits, File file) {
  file.print(":");
  if(digits < 10)
    file.print('0');
  file.print(digits);
}

void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

