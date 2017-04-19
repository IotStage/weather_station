/**
 * Ministaion meteo RF
 * La station recupere 
 */

#include "EEPROM.h"
#include "cc1101.h"
#include "DHT.h"


//************** PIN used for measurement
unsigned int WIND_SPEED_PIN = A1;
unsigned int WIND_VANE_PIN = A2;
unsigned int SOLAR_RADIATION_PIN = A3;
#define DHTPIN 4 


//**************  wind speed parameters
int nb_basc_speed = 0;
unsigned long intervalMesure = 777777;
unsigned long lastTimeMesure = 0, curTimeMesure = 0;
float curWindMesure=0.0, lastWindMesure=0.0;


//********************* dht parameters
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

//******************** RF CC1101 parameters
CC1101 cc1101;
byte syncWord[2] = {199, 0}; 
CCPACKET paquet; 
boolean packetAvailable = false;
unsigned int RFCHANNEL = 1;

void setup() {
  Serial.begin(9600);
  
  // initialisation de l'antenne RF
  cc1101.init();
  cc1101.set_433_GFSK_500_K(); 
  cc1101.setChannel(RFCHANNEL);
  cc1101.disableAddressCheck(); 
  attachInterrupt(0, cc1101signalsInterrupt, FALLING);
  Serial.println("Initialisarion antenne RF terminee...");
  
  dht.begin();
  Serial.println("Initialisarion DHT terminee...");
  initMesure();

}

void loop() {
    curTimeMesure = millis();
    
    while((curTimeMesure-lastTimeMesure) < intervalMesure){
      calculateWindSpeed();
      curTimeMesure = millis();
    }

    //recuperer la vitesse
    float windSpeed = getWindSpeed();
    float windDirection = getWindDirection();
    String windHeading = getHeading(windDirection);
    float temperature = getCTemperature();
    float humidity = getHumidity();
    float solarRadiation = getSolarRadiation();

    String msg = "meteo "+String(windSpeed, 2) + " ";
    msg += String(windDirection, 2) + " ";
    msg += windHeading + " ";
    msg += String(temperature, 2) + " ";
    msg += String(humidity, 2) + " ";
    msg += String(solarRadiation, 2) + " ";

    Serial.println("Nouvelle mesure: "+msg);
   sendRFData(msg);
    
    initMesure();
}

/*
 * Reinitialisation
 */
float initMesure(){

  //initialiser les parametres du capteur de la vitesse du vent
  nb_basc_speed = 0;
  curWindMesure=0.0;
  lastWindMesure=0.0;

  //linstant de mesure
  lastTimeMesure = curTimeMesure;
}

/*
 * 
 */
float getWindSpeed(){
  float vitesse = nb_basc_speed*2.4/intervalMesure;
  return vitesse;
}

/*
 * calculer le nombre dimpulsion du capteur de vent
 */
void calculateWindSpeed(){
  float sensorValue = analogRead(WIND_SPEED_PIN);      
  curWindMesure = (sensorValue*5)/1023;
  if(curWindMesure<2.50 && lastWindMesure>2.5){
     nb_basc_speed++;
  }
  lastWindMesure = curWindMesure;
}

/*
 * Recuperer la direction du vent en degres
 */
float getWindDirection(){
    float res =0.0;
    float sensorValue = analogRead(WIND_VANE_PIN);
    
    float mesure = (sensorValue*5.0)/1023;
    //Serial.println(mesure);

    if(mesure < 4.02 && mesure > 3.74) {
      res = 0;
    }else if (mesure > 1.96 && mesure < 2.10) {
      res = 22.5;
    }else if (mesure > 2.23 && mesure < 2.32) {
      res = 45;
    }else if (mesure > 0.39 && mesure < 0.43) {
      res = 67.5;
    }else if (mesure >= 0.43 && mesure < 0.47) {
      res = 90;
    }else if (mesure > 0.30 && mesure < 0.34) {
      res = 112.5;
    }else if (mesure > 0.88 && mesure < 0.92) {
      res = 135;
    }else if (mesure > 0.60 && mesure < 0.64) {
      res = 157.5;
    }else if (mesure > 1.38 && mesure < 1.42) {
      res = 180;
    }else if (mesure > 1.17 && mesure < 1.21) {
      res = 202.5;
    }else if (mesure > 3.06 && mesure < 3.11) {
      res = 225;
    }else if (mesure > 2.91 && mesure < 2.96) {
      res = 247.5;
    }else if (mesure > 4.6 && mesure < 4.64) {
      res = 270;
    }else if (mesure >= 4.02 && mesure < 4.08) {
      res = 292.5;
    }else if (mesure > 4.76 && mesure < 4.8) {
      res = 315;
    }else if (mesure > 3.41 && mesure <= 3.5) {
      res = 337.5;
    }
    return res;
}

/*
 * 
 */
String getHeading(float direction) { 
  String heading = "";
  if(direction < 22.5) 
   heading = "N"; 
  else if (direction <= 67.5) 
    heading = "NE"; 
  else if (direction < 112.5) 
    heading = "E"; 
  else if (direction <= 157.5) 
    heading = "SE"; 
  else if (direction < 212.5) 
    heading = "S"; 
  else if (direction <= 247.5) 
    heading = "SW"; 
  else if (direction < 292.5) 
    heading = "W"; 
  else if (direction <= 337.5) 
    heading = "NW"; 
  else 
    heading = "N"; 
   return heading;
}

/*
 * temperature en celcuis
 */
float getCTemperature(){
  float t = dht.readTemperature();
  return t;
}

/*
 * humidity
 */
float getHumidity(){
  float h = dht.readHumidity();
  return h;
}


/*
 * radiation solaire en W/m²
 */

 float getSolarRadiation(){
    //analogReference(INTERNAL);
    float solarRadiation = analogRead(SOLAR_RADIATION_PIN);
    solarRadiation = (solarRadiation*5.0/1023)*1000*5*0.327;
    
    return solarRadiation;
 }

/*
 * lorsqu'un paquet est disponible
 */
void cc1101signalsInterrupt(void){
  packetAvailable = true;
}

/*
 * envoie les donnees par RF
 * Le message est decomposé en paquets de taille maximale 55 caracteres
 */
void sendRFData(String msg) {

  if(msg.length()<61){
      paquet.length = msg.length();
      msg.getBytes(paquet.data, msg.length()+1);

      //envoyer
      Serial.print("Message: "+msg);
      if(cc1101.sendData(paquet)){
        Serial.println(" -> success");
      }else{
        Serial.println(" -> fail");
      }

  }else{
     sendRFData(msg.substring(0,55)+"~@]#`");
     sendRFData(msg.substring(55));
  }
 
}
