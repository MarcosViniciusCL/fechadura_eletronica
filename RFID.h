#ifndef RFID_h
#define RFID_h

#define SS_PIN D4
#define RST_PIN D2

#include <SPI.h>
#include <MFRC522.h>

MFRC522 mfrc522(SS_PIN, RST_PIN);


class RFID {
  private:
    String r;
    
  public:
    RFID(){
      //r = new String();
    }
    String loop(){
      return lerRFID();
    }
    void init(){
      mfrc522.PCD_Init();                                       //Iniciando o leitor RFID
    }
    String lerRFID(){
      r = "";
      if ( mfrc522.PICC_IsNewCardPresent()){
            if ( mfrc522.PICC_ReadCardSerial()){ 
               for (byte i = 0; i < mfrc522.uid.size; i++) {
                      r += mfrc522.uid.uidByte[i];
               }
              
                mfrc522.PICC_HaltA();
                Serial.println(r);
                return r;
            }
      }
      return r;
   } 
};

#endif
