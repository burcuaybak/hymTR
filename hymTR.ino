/*
 * hymTR 2020
 * Baris DINC - TA7W
 * APRS Tracker
 */
#include <SoftwareSerial.h>
#include <SimpleTimer.h>
//#include <TinyGPS.h>
#include <Adafruit_GPS.h>
#include <LibAPRS.h>
#include <EEPROM.h>

//Genel Tanimlamalar
#define VERSIYON "ABCD"

// GPS SoftwareSerial
// GPS'in bagli oldugu PIN'ler
#define GPS_RX_PIN 12
#define GPS_TX_PIN 11

//Test butonu manuel gonderme islemi icindir
#define TEST_BUTTON_PIN 10

// PTT_PIN'i libAPRS/device.h icinde tanimlanmistir
#define PTT_PIN 3

// LibAPRS
#define OPEN_SQUELCH false
#define ADC_REFERENCE REF_3V3  // Bu deger Nano icin 3V3 , uno icin 5V olacak

//char mesaj []= "/A=001234 TAMSAT hymTR APRS Tracker";

struct APRS_Ayarlari {
  char APRS_CagriIsareti[7];
  char APRS_CagriIsaretiSSID;
//  char APRS_Destination[7];
//  char APRS_DestinationSSID;  
//  char APRS_Path1[7];
//  char APRS_Path1SSID;
//  char APRS_Path2[7];
//  char APRS_Path2SSID;
  char APRS_Sembolu;
//  char APRS_SembolTabi;
//  byte APRS_BeaconTipi;    //0=sure beklemeli, 1=Smart Beacon
  unsigned long APRS_BeaconSuresi;
//  unsigned int  APRS_GPSSeriHizi;    
  char APRS_Mesaj[11]; //TODO 41 olacak
  unsigned int CheckSum;    //Cagri isaretinin byte toplamini kullaniyoruz
};

APRS_Ayarlari Ayarlar;

// Gonderme zamanlayicisi olarak kullanilan Timer
#define TIMER_DISABLED -1
#define TIMER_MINUTES 10000 //60L*1000L

SoftwareSerial GPSSerial(GPS_RX_PIN, GPS_TX_PIN);
//TinyGPS gps;
Adafruit_GPS GPS(&GPSSerial);

SimpleTimer timer;

char aprs_update_timer_id = TIMER_DISABLED;
bool send_aprs_update = false;

//long lat = 0;
//long lon = 0;
//long alt = 0;
//long gps_course = 0;
//long gps_speed = 0;

//int year=0;
//byte month=0, day=0, hour=0, minute=0, second=0, hundredths=0;
//unsigned long age=0;

bool newData = false;
bool ilkAcilis = true;

//Fonksiyonlarimiz
//void eepromOku();

void setup()  
{
   eepromOku();
  
  Serial.begin(115200);   //Bilgisayar haberlesme portu
  GPSSerial.begin(9600);  //GPS veri okuma portu
  //TODO: ayarlanan parametreyi kullan

  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  pinMode( 13, OUTPUT ); //PIN13????
  
  Serial.println(F("TAMSAT hymTR APRS Tracker"));

  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setPreamble(350); //TODO: Konfigurasyondan kullan
  APRS_setTail(50);      //TODO: Konfigurasyondan kullan
  APRS_setCallsign(Ayarlar.APRS_CagriIsareti,Ayarlar.APRS_CagriIsaretiSSID);
  APRS_setSymbol(Ayarlar.APRS_Sembolu);
  
//  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
//  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
//  GPS.sendCommand(PGCMD_ANTENNA);  
  
  aprs_update_timer_id=timer.setInterval(2*TIMER_MINUTES, setAprsUpdateFlag);
}

void loop()
{
    /*
     * Burada GPS datalarini okuyup anlamli bir veri olup olmadigina bakiyoruz
     */
    while (GPSSerial.available())
    {
      char c = GPS.read(); //GPSSerial.read();
      //Serial.write(c); 
      //if (gps.encode(c)) newData = true;
      if (GPS.newNMEAreceived()) GPS.parse(GPS.lastNMEA());
    }

    
/*
   if (newData)// & !send_aprs_update)
    {
    //gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, NULL, &age);
    gps.get_position(&lat, &lon, &age);
    alt = gps.altitude() / 3.281;
    gps_course = gps.course();
    gps_speed = gps.speed();
    newData = false; //data islendi ve aktarildi
    }  
*/

  if (send_aprs_update || digitalRead(TEST_BUTTON_PIN)==0) {
    while(digitalRead(TEST_BUTTON_PIN)==0) {};
    lokasyonGuncelle();
    newData = false;
//    Serial.println("new data");
//    Serial.println(lat);
//    Serial.println(alt);
  }

  /*
   * APRS Timer saati...
   */
  timer.run();

  /*
   * Burada seri porttan komutlari okuyoruz
   * TODO: bunu interrupt a tasimaliyiz
   */
  while (Serial.available())
  {
    char komut = Serial.read();
     if (komut == '!') {
      KomutSatiriCalistir();
      }
  }


}

void aprs_msg_callback(struct AX25Msg *msg) {
}

void lokasyonGuncelle() {
  
  //String alt = alt + 1000000;
  
  APRS_setLat((char*)deg_to_nmea(GPS.latitude, true));
  APRS_setLon((char*)deg_to_nmea(GPS.longitude, false));
      
  GPSSerial.end();
  // TX
  APRS_sendLoc(Ayarlar.APRS_Mesaj, strlen(Ayarlar.APRS_Mesaj));
  while(digitalRead(PTT_PIN));
  GPSSerial.begin(9600); //TODO: ayarlanani kullan
  send_aprs_update = false;
}

char* deg_to_nmea(long deg, boolean is_lat) {
  #define CONV_BUF_SIZE 16
  static char conv_buf[CONV_BUF_SIZE];
  bool is_negative=0;
  if (deg < 0) is_negative=1;

  // Use the absolute number for calculation and update the buffer at the end
  deg = labs(deg);

  unsigned long b = (deg % 1000000UL) * 60UL;
  unsigned long a = (deg / 1000000UL) * 100UL + b / 1000000UL;
  b = (b % 1000000UL) / 10000UL;

  conv_buf[0] = '0';
  // in case latitude is a 3 digit number (degrees in long format)
  if( a > 9999) {
    snprintf(conv_buf , 6, "%04lu", a);
  } else {
    snprintf(conv_buf + 1, 5, "%04lu", a);
  }

  conv_buf[5] = '.';
  snprintf(conv_buf + 6, 3, "%02lu", b);
  conv_buf[9] = '\0';
  if (is_lat) {
    if (is_negative) {conv_buf[8]='S';}
    else conv_buf[8]='N';
    return conv_buf+1;
    // conv_buf +1 because we want to omit the leading zero
    }
  else {
    if (is_negative) {conv_buf[8]='W';}
    else conv_buf[8]='E';
    return conv_buf;
    }
}

void setAprsUpdateFlag() {
  send_aprs_update = true;
}

/*
 * Config Struct ina uygun sayida veriyi sirayla EEPROM'dan okuyup 
 * bu struct a uygun degiskenin icine atiyoruz
 */
void eepromOku() {
  for (unsigned int i=0; i<sizeof(Ayarlar); i++) {
    *((char*)&Ayarlar + i) = EEPROM.read(i);
  }
  /*Cagri isareti dogru ise hepsinin dogru oldugunu kabul ediyoruz*/
  unsigned int iCheckSum = 0;
  for (int i=0; i<7; i++) {
    iCheckSum += Ayarlar.APRS_CagriIsareti[i];
  }
  if (iCheckSum != Ayarlar.CheckSum)  VarsayilanAyarlar();
  Serial.println(F("EEPROM'dan okuma tamamlandi"));
}


void VarsayilanAyarlar() {
  strcpy(Ayarlar.APRS_CagriIsareti, "TAMSAT");
  Ayarlar.APRS_CagriIsaretiSSID = '9';
/*  strcpy(Ayarlar.APRS_Destination, "APRS  ");
  Ayarlar.APRS_DestinationSSID = '0';
  strcpy(Ayarlar.APRS_Path1, "WIDE1 ");
  Ayarlar.APRS_Path1SSID = '1';
  strcpy(Ayarlar.APRS_Path2, "WIDE2 ");
  Ayarlar.APRS_Path2SSID = '1';
*/  
  Ayarlar.APRS_Sembolu = '>';
//  Ayarlar.APRS_SembolTabi = '/';
//  Ayarlar.APRS_BeaconTipi = 3;
  Ayarlar.APRS_BeaconSuresi = 255;
  strcpy(Ayarlar.APRS_Mesaj, "hymTR");
//  Ayarlar.APRS_GPSSeriHizi = 9600;
  Ayarlar.CheckSum = 458;  //TAMSAT icin Checksum degeri
  eepromaYaz();
}

/*
 * CONFIG tipinde bir Struct i EEPROM a yaziyoruz
 */
void eepromaYaz() {
  for (unsigned int i=0; i<sizeof(Ayarlar); i++) {
    EEPROM.write(i, *((char*)&Ayarlar + i));
  }
  //Serial.println("Eeproma yazildi");
}

/*
 * Print configuration details to serial port
 */
void konfigurasyonYazdir()
{
  /*
  Serial.println("Mevcut Konfigurasyon Bilgileri");
  Serial.println("------------------------------");
  */
/*  
  //Serial.print("Cagri Isareti : ");  
  Serial.println(Ayarlar.APRS_CagriIsareti);
  //Serial.print("         SSID : -"); 
  Serial.println(Ayarlar.APRS_CagriIsaretiSSID);
  //Serial.print("        Hedef : ");  
  Serial.println(Ayarlar.APRS_Destination);
  //Serial.print("   Hedef SSID : -"); 
  Serial.println(Ayarlar.APRS_DestinationSSID);  
  //Serial.print("        Path1 : ");  
  Serial.println(Ayarlar.APRS_Path1);
  //Serial.print("   Path1 SSID : -"); 
  Serial.println(Ayarlar.APRS_Path1SSID);
  //Serial.print("        Path2 : ");  
  Serial.println(Ayarlar.APRS_Path2);
  //Serial.print("   Path2 SSID : -"); 
  Serial.println(Ayarlar.APRS_Path2SSID);
  //Serial.print("       Sembol : ");  
  Serial.println(Ayarlar.APRS_Sembolu);
  //Serial.print("  Sembol Tipi : ");  
  Serial.println(Ayarlar.APRS_SembolTabi);
  //Serial.print("  Beacon Tipi : ");  
  Serial.println(Ayarlar.APRS_BeaconTipi); //0=sure beklemeli, 1=Smart Beacon
  //Serial.print("Beacon Suresi : ");  
  Serial.println(Ayarlar.APRS_BeaconSuresi);
  //Serial.print("     GPS Hizi : ");  
  Serial.println(Ayarlar.APRS_GPSSeriHizi);    
  //Serial.print("        Mesaj : ");  
  Serial.println(Ayarlar.APRS_Mesaj);
  //Serial.print("Kontrol (CRC) : ");  
  Serial.println(Ayarlar.CheckSum);   
*/  
}

void KomutSatiriCalistir() {
  byte komut;

  Serial.println(F("hymTR Konfigurasyon Arayuzu"));
  Serial.print(F("#"));
  //delay(50);

  while (komut != 'Q') {
    //digitalWrite(13, !digitalRead(13)); //Gonderme yaptigimizda tracker i konsol modundan cikariyoruz
    //delay(50);
    if (Serial.available()) {
      komut = Serial.read();

      if (komut == 'R') {
        eepromOku();    
        seriyeGonder();
      }

      if (komut == 'P') {
        konfigurasyonYazdir();    
      }

      if (komut == 'W') {
        Serial.print(F("Cihaz Versiyon:"));
        Serial.println(VERSIYON);
        if (seridenAl()) {
              eepromaYaz();
            } else {
          //Serial.println(F("hata  olustu..."));
        }
      }
      
      if (komut == 'D') {
        Serial.println(F("Varsayilan konfigurasyona donuluyor"));
        VarsayilanAyarlar();        
      }
      Serial.println('#');
    } //Serial.available
 } //komut != Q
 Serial.println(F("Konfigurasyon midundan cikiliyor"));
} //komutSatiriCalistir


/*
 * seri olarak okunan byte lari 0x09 veya 0x04 e kadar olanini geri dondurmek icin
 */
void parametreOku(char *szParam, int iMaxLen) {
  byte c;
  int iSize;
  unsigned long iMilliTimeout = millis() + 1000; 

  for (iSize=0; iSize<iMaxLen; iSize++) szParam[iSize] = 0x00; 
  iSize = 0;   

  while (millis() < iMilliTimeout) {

    if (Serial.available()) {
      c = Serial.read();
      if (c == 0x09 || c == 0x04) {
        return;
      }
      if (iSize < iMaxLen) {
        szParam[iSize] = c;
        iSize++;
      }
    }
  }

}

bool seridenAl() {
  char szParam[45]; //en uzun mesaajdan daha uzun 45>41
  unsigned long iMilliTimeout = millis() + 10000;    
  while (millis() < iMilliTimeout) {
  while (!Serial.available()) { } //veri gelmesini bekle
    if (Serial.read() == 0x01) {
      parametreOku(szParam, sizeof(szParam));
      if (strcmp(szParam, VERSIYON) != 0) {
        Serial.print(F("PC:"));
        Serial.println(szParam);
          Serial.println(F("E99 Versiyonlar uyumsuz..."));
        return false;
      }
    
      parametreOku(szParam, sizeof(Ayarlar.APRS_CagriIsareti));    //CagriIsareti
      strcpy(Ayarlar.APRS_CagriIsareti, szParam);
      parametreOku(szParam, 1);    //CagriIsareti SSID
      Ayarlar.APRS_CagriIsaretiSSID = szParam[0];

/*
      parametreOku(szParam, sizeof(Ayarlar.APRS_Destination));    //Destination
      strcpy(Ayarlar.APRS_Destination, szParam);
      parametreOku(szParam, 1);    //SSID
      Ayarlar.APRS_DestinationSSID = szParam[0];

      parametreOku(szParam, sizeof(Ayarlar.APRS_Path1));    //Path1
      strcpy(Ayarlar.APRS_Path1, szParam);
      parametreOku(szParam, 1);    //SSID
      Ayarlar.APRS_Path1SSID = szParam[0];

      parametreOku(szParam, sizeof(Ayarlar.APRS_Path2));    //Path2
      strcpy(Ayarlar.APRS_Path2, szParam);
      parametreOku(szParam, 1);    //SSID
      Ayarlar.APRS_Path2SSID = szParam[0];
*/
      //Symbol/Tab
/*      parametreOku(szParam, 1);
      Ayarlar.APRS_Sembolu = szParam[0];
      parametreOku(szParam, 1);
      Ayarlar.APRS_SembolTabi = szParam[0];

      //BeaconTipi
      parametreOku(szParam, sizeof(szParam));
      Ayarlar.APRS_BeaconTipi = atoi(szParam);
*/
      //Beacon Suresi
      parametreOku(szParam, sizeof(szParam));
      Ayarlar.APRS_BeaconSuresi = atoi(szParam);

      //Status Message
      parametreOku(szParam, sizeof(szParam));
      strcpy(Ayarlar.APRS_Mesaj, szParam);


//      parametreOku(szParam, sizeof(szParam));
//      Ayarlar.APRS_GPSSeriHizi = atoi(szParam);    


      unsigned int iCheckSum = 0;
      for (int i=0; i<7; i++) {
        iCheckSum += Ayarlar.APRS_CagriIsareti[i];
      }
      Ayarlar.CheckSum = iCheckSum;
      return true;    //OKuma tamamlandi
    } // read 0x01
  } //millis
  return false;

}

/*
 * Elimizdeki CONFIG degiskenini byte byte PC'ye gonderiyoruz
 */
void seriyeGonder() {

        Serial.print(F("{"));    //JSON Baslangici
        Serial.print(F("'V':'"));
        Serial.print(VERSIYON);
        Serial.print(F("','CagriIsareti':'"));
        Serial.print(Ayarlar.APRS_CagriIsareti);
        Serial.print(F("','CagriIsaretiSSID':"));
        Serial.print(Ayarlar.APRS_CagriIsaretiSSID);
        Serial.print(F(",'Destination':'"));
/*        Serial.print(Ayarlar.APRS_Destination);
        Serial.print(F("','DestinationSSID':"));
        Serial.print(Ayarlar.APRS_DestinationSSID);
        Serial.print(F(",'Path1':'"));
        Serial.print(Ayarlar.APRS_Path1);
        Serial.print(F("','Path1SSID':"));
        Serial.print(Ayarlar.APRS_Path1SSID);
        Serial.print(F(",'Path2':'"));
        Serial.print(Ayarlar.APRS_Path2);
        Serial.print(F("','Path2SSID':"));
        Serial.print(Ayarlar.APRS_Path2SSID);
*/        //Symbol
        Serial.print(F(",'Sembol':'"));
        Serial.print(Ayarlar.APRS_Sembolu);
//        Serial.print(F("','SembolTabi':'"));
//        Serial.print(Ayarlar.APRS_SembolTabi);
        //Beacon Type
//        Serial.print(F("','BeaconTipi':"));
//        Serial.print(Ayarlar.APRS_BeaconTipi, DEC);
        Serial.print(F(",'BeaconSuresi':"));
        //Beacon - Simple Delay
        Serial.print(Ayarlar.APRS_BeaconSuresi, DEC);
        //Status Message
        Serial.print(F(",'Mesaj':'"));
        Serial.print(Ayarlar.APRS_Mesaj);
        //GPS Serial Data
        Serial.print(F("'"));
//        Serial.print(F("','GPSHizi':"));
//        Serial.print(Ayarlar.APRS_GPSSeriHizi, DEC);      
        Serial.print(F("}")); //JSON Sonu 

}
