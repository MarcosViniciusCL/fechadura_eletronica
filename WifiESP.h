#ifndef WifiESP_h
#define WifiESP_h

#include <ESP8266WiFi.h>
#include <EEPROM.h>



class WifiESP {
  private:
    String ssid;
    String password;
    char *state;
    long int  MAX_TIME = 1000;
    long int counter;

  public:
    WifiESP(char *s){
      state = s;
      counter = 0;
      WiFi.mode(WIFI_STA);
    }
    void connectWifi() {
      if (counter >= MAX_TIME){
        Serial.println(WL_CONNECTED);
        Serial.println(WiFi.status());
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println(counter);
        counter=0;
        *state = 'W';
        if(WiFi.status() == WL_CONNECTED){
          *state = 'A';
          Serial.println("Conectado à " + ssid);
        }
      }
      
      counter++;
      if(counter > MAX_TIME) counter=0;
    }

    void setSSID(String s) {
      ssid = s;
    }

    String getSSID() {
      return ssid;
    }

    void setPassword(String p) {
      password = p;
    }

    String getPassword() {
      return password;
    }

    int getStatus(){
        return WiFi.status();
    }

    void desconectar(){
      WiFi.disconnect();
    }

    void saveMemory(){
      EEPROM.put(0, ssid);
      EEPROM.put(34, password);
      EEPROM.commit();
    }

    void loadMemory(){
      EEPROM.get(0, ssid);
      EEPROM.get(34, password);
      Serial.println(ssid);
    }
    void loop(){
        if(WiFi.status() != WL_CONNECTED){
            connectWifi();
        }
    }
};

#endif
