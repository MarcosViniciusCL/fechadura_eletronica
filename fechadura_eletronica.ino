
#define SS_PIN D4
#define RST_PIN D3

#define DEBUG 1


#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <ArduinoWebsockets.h>


String id_device = "";
String ssid;//     = STASSID;
String password;// = STAPSK;



#define RELE D2
#define BUZZ D1

#define WAIT_TIME_WEBSOCKET 5                     // Tempo de tentativa de conexao do mqtt em segundos
#define WAIT_TIME_WIFI 10                     // Tempo de tentativa de conexao do wifi em segundos
#define CHECK_TIME 9                          // Tempo entre cada verificação de conexao, em segundos

using namespace websockets;


String PIN = "";

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key; 


//StaticJsonDocument<1024> doc;
DynamicJsonDocument doc(2048);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

WebsocketsClient clientWebSocket;

String last_tag = "";

char _websocket_conectado = 0;
char _wifi_conectado = 0;
unsigned long mqtt_time_prev = 0;
unsigned long wifi_time_prev = 0;
unsigned long rfid_time_prev = 0;
unsigned long check_time_prev = 0;

typedef struct m {                                          // Estrutuda de dados para salvar as configurações do servidor mqtt
  String server;
  int port;
} conf_websocket;

conf_websocket websocket;

typedef struct n {
    String tag;
    long int timestamp;
} last_done;

last_done tag_done;


void setup() {
    initESP();
    delay(3000);
    buzzer_done();
}

void initESP(){
    char filesystem = -1;
    Serial.begin(115200);
    delay(3000);
    Serial.println("\n\nSEJA BEM VINDO - UNLOCKED");
    Serial.println("Iniciando saidas do sistema...");
    pinMode(RELE, OUTPUT);
    digitalWrite(RELE, HIGH);
    pinMode(BUZZ, OUTPUT);
    digitalWrite(BUZZ, LOW);
    Serial.println("Iniciando sistema de arquivos...");
    /* ###### SPIFFS ###### */
    if(!SPIFFS.begin()){
        Serial.println("\nErro ao abrir o sistema de arquivo...");
        while (1);
    }
    File rFile = SPIFFS.open("/CONFIG","r");
    if (!rFile) { // Será executado apenas se for a primeira vez que estiver usando o sistema
        Serial.println("Configuração necessaria na primeira execução.\nDeseja configurar agora? [S/n]: ");
        String r = lerSerial();
        r.toLowerCase();
        while(!r.equals("s")){
            Serial.println("Desculpe, mas para usar tem que configurar. Vamos agora? [S/n]: ");
            r = lerSerial();
            r.toLowerCase();
        }
        Serial.println("Formatando memoria....");
        SPIFFS.format();
        String conf;
        char reenviar_conf = 0;
        do {
            reenviar_conf = 0;
            Serial.println("Pronto. Preciso que voce envie um arquivo de configuracao. [Sobre: https://github.com/MarcosViniciusCL/fechadura_eletronica]");
            char erro = -1;
            while(erro != 0){
                erro = 0;
                conf = lerSerial();
                //conf.replace(" ", "");
                DeserializationError err = deserializeJson(doc, conf);
                if (err) {
                    Serial.print(F("deserializeJson() failed with code "));
                    Serial.println(err.c_str());
                    erro = 1;
                    Serial.println("Houve um erro no arquivo enviado.\nVerifique o link para informacoes para fazer o arquivo, e envie novamente.\n[Sobre: https://github.com/MarcosViniciusCL/fechadura_eletronica]");
                }
            }
            // Exibindo os dados enviado para o usuario aceitar.
            auto id = doc["id_device"].as<char*>();

            auto ssid = doc["network"]["wifi"][0].as<char*>();
            auto wifi_password = doc["network"]["wifi"][1].as<char*>();

            auto websocket_server = doc["network"]["websocket"]["server"].as<char*>();
            auto websocket_port = doc["network"]["websocket"]["port"].as<char*>();
            

            auto pin = doc["system"]["password"].as<char*>();

            Serial.println("\n\n**********************");
            Serial.println("ID DEVICE: " + String(id));
            Serial.println("NETWORK:");
            Serial.println("ssid: " + String(ssid) + "\nsenha: " + String(wifi_password));
            Serial.println("\nWEBSOCKET:");
            Serial.println("server: " + String(websocket_server) + "\nport: " + String(websocket_port));
            Serial.println("\nSYSTEM:");
            Serial.println("password: " + String(pin));
            Serial.println("**********************\n\n");
    
            Serial.println("Salvar configuracao? [S/n]: ");
            r = lerSerial();
            r.toLowerCase();
            if(r.equals("s")){
                File rFile = SPIFFS.open("/CONFIG","w+");
                rFile.println(conf); 
                rFile.close();
                changeWifi(String(ssid), String(wifi_password));
                Serial.println("Configuracao foi salva.");
            } else {
                Serial.println("Nada foi salvo. Reenvie o arquivo...");
                reenviar_conf = 1;
                delay(1000);
            }
        } while (reenviar_conf == 1);
    }

    Serial.println("Iniciando modulo de leitor de cartao....");
    // ##### RFID CONFIG #### 
    SPI.begin();
    rfid.PCD_Init();
    rfid.PCD_WriteRegister(MFRC522::ComIrqReg, 0x80); //Clear interrupts
    rfid.PCD_WriteRegister(MFRC522::ComIEnReg, 0x7F); //Enable all interrupts
    rfid.PCD_WriteRegister(MFRC522::DivIEnReg, 0x14);
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println("Carregando configuracoes...");
    upConfig();

    Serial.println("Configurando modo de trabalho do WIFI...");
    // ###### WiFi CONFIG ######### 
    WiFi.mode(WIFI_STA);

    Serial.println("Configurando dados WEBSOCKET...");

    // ###### MQTT CONFIG ######### 

    Serial.println("Iniciando cliente NTP...");
    timeClient.begin();


    Serial.println("SISTEMA INICIADO\n\n");

}

String lerSerial(){
    while (Serial.available() <= 0);
    return Serial.readStringUntil('\n');
}

void changeWifi(String s, String p) {
  ssid = s;
  password = p;
  WiFi.disconnect();
}


void upConfig(){
    File rFile = SPIFFS.open("/CONFIG","r");
    if (!rFile) {
        msg("Erro nas configuracoes do sistema. Sistema de ser formatado.");
        if(lerSerial().equals("s")){
            msg("Formatando memoria e reiniciando....");
            SPIFFS.format();
            ESP.reset();
        }
        while (1) delay(100);
    }
    String content = rFile.readStringUntil('\r'); //desconsidera '\r\n'
    rFile.close();
    DeserializationError err = deserializeJson(doc, content);
    ssid = String(doc["network"]["wifi"][0].as<char*>());
    //auto wifi_password = doc["network"]["wifi"][1].as<char*>();

    // ###### MQTT CONFIG ######### 
    websocket.server = String(doc["network"]["websocket"]["server"].as<char*>());
    websocket.port = doc["network"]["websocket"]["port"].as<int>();

    PIN = doc["system"]["password"].as<char*>();
    id_device = String(doc["id_device"].as<char*>());
}

void publicar(String msg){
    //client.publish((mqtt.subTopic + "/output").c_str(), msg.c_str());
    clientWebSocket.send(msg.c_str());
}


String lerRFID(){
  if((millis() - rfid_time_prev) >= 50){
    //Serial.println(".");
    rfid_time_prev = millis();
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if ( ! rfid.PICC_IsNewCardPresent())
      return "";
  
    // Verify if the NUID has been readed
    if ( ! rfid.PICC_ReadCardSerial())
      return "";
  
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    //Serial.println(rfid.PICC_GetTypeName(piccType));
  
    // Check is the PICC of Classic MIFARE type
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      msg(F("Your tag is not of type MIFARE Classic."));
      return "";
    }
    String r = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      r += rfid.uid.uidByte[i];
    }
    // Halt PICC
    rfid.PICC_HaltA();
  
    // Stop encryption on PCD
    rfid.PCD_StopCrypto1();
    
    return r;
  }
  return "";
} 


void abrir_porta(){
    buzzer_done();
    digitalWrite(RELE, LOW);
    delay(500);
    digitalWrite(RELE, HIGH);
    msg("Publicando...");
    publicar("Aberto");
}



long int websocket_time_prev = 0;
void connect_websocket(){
    if ((millis() - websocket_time_prev) > WAIT_TIME_WEBSOCKET*1000 && _wifi_conectado){
        websocket_time_prev = millis();
        msg("WEBSOCKET: CONETANDO... [" + websocket.server + ":" + websocket.port + "]");
        // Tentamos conectar com o websockets server
        bool connected = clientWebSocket.connect(websocket.server.c_str(), websocket.port , "/");
    
        // Se foi possível conectar
        if(connected) {
            // Exibimos mensagem de sucesso
            msg("WEBSOCKET: CONECTADO");
            _websocket_conectado = 1;
            // Enviamos uma msg "Hello Server" para o servidor
            clientWebSocket.send("{\"id_device\":\"" + id_device + "\",\"msg\":\"hello server\"}");
        }   // Se não foi possível conectar
        else {
            // Exibimos mensagem de falha
            msg("WEBSOCKET: NÃO CONECTADO");
            _websocket_conectado = 0;
            return;
        }
        // Iniciamos o callback onde as mesagens serão recebidas
        clientWebSocket.onMessage(callback_websocket);
    }
}

void callback_websocket(WebsocketsMessage message){
    // Exibimos a mensagem recebida na serial
    msg(message.data());
    checkCommand(message.data());
}

void conectar_wifi(){
  _wifi_conectado = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  if (_wifi_conectado == 1){
    Serial.println("WIFI: CONECTADO ["+WiFi.SSID()+"]");
    //setupOta();
    return;
  }
  if((millis()-wifi_time_prev) >= (WAIT_TIME_WIFI*1000)){
    wifi_time_prev = millis();
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("WIFI: CONECTANDO... ["+WiFi.SSID()+"]");
  }
}

void verif_conexao(){
  if((millis() - check_time_prev) >= (CHECK_TIME*1000)){
    check_time_prev = millis();
    _wifi_conectado = (WiFi.status() == WL_CONNECTED) ? 1 : 0;      // Verifica se tem conexão com a internet
    //int mqtt_state = client.state();
    //_mqtt_conectado = (mqtt_state == 0) ? 1 : 0;                 // Verifica se o MQTT ta conectado
    //if (client.state() != 0 && _wifi_conectado) {
    //    mostrarErroMqtt(client.state());
    //}
  }
}

void msg(String msg){
    Serial.println(diasHoras() + " -> " + msg);
}

String diasHoras(){
    String dias[7]={"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sabado"};
    return  (_wifi_conectado) ? dias[timeClient.getDay()] + ", " + timeClient.getFormattedTime() : "S/H";
}


long int tempo_buzzer = 0;
long int vezes_buzzer = 0;
long int tempo_buzzer_on = 0;
void buzzer_loop(){
    if(millis() - tempo_buzzer_on > 1500){
        tempo_buzzer_on = millis();
        if(!_wifi_conectado || !_websocket_conectado){
            digitalWrite(BUZZ, HIGH);
            delay(50);
            digitalWrite(BUZZ, LOW);
        } else {
            digitalWrite(BUZZ, LOW);
        }
        
    }
}
void buzzer(long int ms, int q){
    tempo_buzzer = ms;
    vezes_buzzer = q;
}

void buzzer_done(){
    digitalWrite(BUZZ, HIGH);
    delay(50);
    digitalWrite(BUZZ, LOW);
    delay(100);
    digitalWrite(BUZZ, HIGH);
    delay(50);
    digitalWrite(BUZZ, LOW);
}

void buzzer_error(){
    digitalWrite(BUZZ, HIGH);
    delay(250);
    digitalWrite(BUZZ, LOW);
}


void solicitarAbertura(String tag){
    String msg = " {\"type\":\"question\",\"timestamp\":\"" + String(millis()) + "\",\"data\":{\"cmd\":\"open\",\"tag\":\"" + tag + "\"}}";
    publicar(msg);
}
/* ################################### CONFIGURAÇÃO DO SISTEMA E USUARIOS ##########################################*/

bool _root = false;


void clearEEPROM() {
  msg("Aguarde. Limpando...");
  SPIFFS.format();
  msg("Limpeza concluída.");
}

void restartEsp() {
  msg("Reiniciando sistema...");
  ESP.restart();
}

void checkCardToOpen(String tag){
    last_tag = tag;
    solicitarAbertura(tag);
}

void checkCommand(String mensagem) {
  if (!mensagem.equals("")) {
    mensagem.trim();
    DeserializationError err = deserializeJson(doc, mensagem);
    if (err) {
        publicar("deserializeJson() failed with code " + String(err.c_str()));
        return;
    }
    /* Verifica se a mensagem recebida do servidor já tem mais de 10 segundos. 
    * Caso esteja antiga, a mensagem é descartada. Obs: Apenas para mensagem de resposta.
    */
    if (String(doc["type"].as<char*>()).equals("response") && (millis() - doc["timestamp"].as<long int>() > 10000)){
        publicar("Pacote ignorado por demora na resposta.");
        return;
    }
    /* Abrir portao caso a solicitação de abertura tenha sido enviada para o servidor 
    * Vai ser verificado se é uma resposta do servidor e se a tag corresponde a anterior
    */ 
    if(String(doc["type"].as<char*>()).equals("response") && String(doc["data"]["tag"].as<char*>()).equals(last_tag) && String(doc["data"]["cmd"].as<char*>()).equals("open")){
        if (tag_done.tag.equals(last_tag) && millis() - tag_done.timestamp < 2000) {
            publicar("Mensagem duplicada");
            return;
        }
        tag_done.tag = last_tag;
        tag_done.timestamp = millis();
        abrir_porta();
    }
    if(String(doc["type"].as<char*>()).equals("response") && String(doc["data"]["tag"].as<char*>()).equals(last_tag) && String(doc["data"]["cmd"].as<char*>()).equals("close")){
        buzzer_error();
    }
    if(String(doc["type"].as<char*>()).equals("command") && String(doc["data"]["cmd"].as<char*>()).equals("open")){
        abrir_porta();
    }
    if (String(doc["type"].as<char*>()).equals("ping")) {
        clientWebSocket.send("{\"id_device\":\""+ id_device +"\",\"type\":\"pong\"}");
    }
  }
}

long int time_loop = 0;

void loop() {
    delay(100);
    
    if(_wifi_conectado != 1){
        conectar_wifi();
    }

    if (!clientWebSocket.available()) {
        connect_websocket();
    }
    
    String v = lerRFID();
    if(!v.equals("")){
        msg("V: " + v);
        checkCardToOpen(v);
    }
    verif_conexao();
    timeClient.update();
    clientWebSocket.poll();
    buzzer_loop();
}
