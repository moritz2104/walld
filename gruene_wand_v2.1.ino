#include <SPI.h>
#include <SD.h>
#include <DHT.h>


#include <ThreeWire.h>  
#include <RtcDS1302.h>

#define SENSORPIN A0
#define DHTTYPE DHT11    // Es handelt sich um den DHT11 Sensor


const unsigned int sdCard = 5;
const unsigned int pumpePin = 2;
const unsigned int lichtPin = 7;
const unsigned dhtPin = 3;
DHT dht(dhtPin, DHTTYPE);


const int kCePin = 10; // RST Pin Chip Enable
const int kIoPin = 9; // Input/Output
const int kSclkPin = 8; // Serial Clock

ThreeWire myWire(kIoPin, kSclkPin, kCePin); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

int sensorWert = 0;

const unsigned long messungsInterval_UL = 60u*60u*1000;//10*1000; // 24*60*60*1000;  // 1x am Tag messen und rausschreiben

const unsigned int pumpdauer = 20; // in Sekunden
const unsigned int pause = 60; // in Sekunden
const unsigned int schwellenwert = 450; // Sensowert steigt mit zunehmender Trockenheit

const unsigned int lichtAn = 8; // 9 uhr geht licht an
const unsigned int lichtAus = 18; // 17 uhr geht licht an

void setup() {
  Serial.begin(9600);               // Start der seriellen Kommunikation
  pinMode(pumpePin,OUTPUT);
  pinMode(lichtPin,OUTPUT);
  dht.begin(); //DHT11 Sensor starten

  if (!SD.begin(sdCard)) {                                // Wenn die SD-Karte nicht (!SD.begin) gefunden werden kann, ...
    Serial.println("Initialisierung fehlgeschlagen!");    // ... soll eine Fehlermeldung ausgegeben werden. ....
    return;
  }

  // setup Real Time Clock
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.println(printDateTime(compiled));
  
  if (!Rtc.IsDateTimeValid()) 
  {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing
  
      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
  }
  
  if (Rtc.GetIsWriteProtected())
  {
      Serial.println("RTC was write protected, enabling writing now");
      Rtc.SetIsWriteProtected(false);
  }
  
  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }
  
  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

}

// niedrige Werte --> nasse Erde,
// hohe Werte --> trockene Erde
// ohne Erde rund 500 - 600
// komplett nass --> ca 250
// TODO sehr trocken --> ???

void loop()
{
 
  static unsigned long zeitSeitMessung =  - messungsInterval_UL;  // initialize such that a reading is due the first time through loop()
  static bool licht = false;
  
  String dataString = "";
  
  unsigned long jetzt = millis();

  // Messen
  int sensorWert = sensorWertMittel();
  float luftfeuchtigkeit = dht.readHumidity(); //die Luftfeuchtigkeit auslesen und unter „Luftfeutchtigkeit“ speichern
  float temperatur = dht.readTemperature(); //die Temperatur auslesen und unter „Temperatur“ speichern
  
  if (jetzt - zeitSeitMessung >= messungsInterval_UL){
    zeitSeitMessung += messungsInterval_UL;
    RtcDateTime timenow = Rtc.GetDateTime();
    dataString = printDateTime(timenow)+","+String(sensorWert) + "," + String(luftfeuchtigkeit) + "," + String(temperatur);
    if(!writeOutData(String("datalog.txt"), dataString)){
      Serial.println("could not write no SD Card");
      Serial.println(dataString);
    }
  }
  
  // Wässern
  dataString = "";
  if(sensorWert>schwellenwert){
    RtcDateTime timenow = Rtc.GetDateTime();
    dataString = printDateTime(timenow) + " pumpdauer:" + String(pumpdauer) + " pause:" + String(pause) + " neue Sensorwerte:";
  }
  while (sensorWert>schwellenwert) { // wenn der Sensor über dem Schwellenwert liegt so lange gießen und warten bis er wieder drüber ist
    digitalWrite(pumpePin, HIGH);
    delay(pumpdauer*1000); 
    digitalWrite(pumpePin, LOW);
    delay(pause*1000);
    sensorWert = sensorWertMittel();  // lies den aktuellen Sensorwert nach dem Wässern aus
    dataString += " " + String(sensorWert);
    Serial.print("neuer Sensorwert: \t"); Serial.println(sensorWert); // Ausgabe des Wertes
  }
  if(dataString.length()){
    if(!writeOutData(String("pumpelog.txt"), dataString)){
      Serial.println("could not write, no SD Card");
      Serial.println(dataString);
    }
  }

  //licht
  RtcDateTime timenow = Rtc.GetDateTime();
  if(!licht && timenow.Hour() >= lichtAn && timenow.Hour() < lichtAus){
    licht = true;
    digitalWrite(lichtPin, HIGH);
    Serial.println("Licht An");
  }

  if(licht && timenow.Hour() >= lichtAus){
    licht = false;
    digitalWrite(lichtPin, LOW);
    Serial.println("Licht Aus " + String(timenow.Hour()));
  }
  
  delay(1000); 
  
// TODO: wenn kein Wasser im Tank, dann nicht wässern
 
}


bool writeOutData(String dateiName, String dataString){
  File dataFile = SD.open(dateiName.c_str(), FILE_WRITE);

  if(!dataFile){
    // try to reinitialize
    if (!SD.begin(sdCard)) {                                
      Serial.println("Initialisierung fehlgeschlagen!");
      return false;
    } else{
      dataFile = SD.open(dateiName.c_str(), FILE_WRITE);
    }
  }
  if (dataFile) {

    dataFile.println(dataString);

    dataFile.close();

    // print to the serial port too:

    Serial.println(dataString);

    return true;
  }
  return false;
}


int sensorWertMittel(){
  float value = 0;
  for(int i=0;i<10; i++){
    value += analogRead(SENSORPIN); // und lies den aktuellen Sensorwert vorm dem Wässern aus
    delay(10);
  }
  value /=10.0;
  return int(value);
}


#define countof(a) (sizeof(a) / sizeof(a[0]))
String printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    return String(datestring);
}
