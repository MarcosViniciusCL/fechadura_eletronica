
#define SS_PIN D4
#define RST_PIN D3

#define DEBUG 1


#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <StringSplitter.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//#include <AESLib.h>

#define PIN_OFFSET 0
#define DATA_OFFSET 5


#ifndef STASSID
//#define STASSID "Cavalo de Troia.exe";
#define STASSID "NOTE";
#define STAPSK  "@n3tworking";
#endif

String ssid;//     = STASSID;
String password;// = STAPSK;

String server = "www.google.com.br";
int porta = 80;


#define RELE D2
#define BUZZ D1

#define WAIT_TIME_MQTT 15                     // Tempo de tentativa de conexao do mqtt em segundos
#define WAIT_TIME_WIFI 10                     // Tempo de tentativa de conexao do wifi em segundos
#define CHECK_TIME 9                          // Tempo entre cada verificação de conexao, em segundos

unsigned char MQTT_ENABLE = 1;

#define VECTOR_SIZE_MAX 3
String secure_card[VECTOR_SIZE_MAX+1][2];
int INDEX_ROOT = VECTOR_SIZE_MAX + 1;

String PIN = "";

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key; 

WiFiClient espClient;
PubSubClient client(espClient);

//StaticJsonDocument<1024> doc;
DynamicJsonDocument doc(2048);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

char _mqtt_conectado = 0;
char _wifi_conectado = 0;
unsigned long mqtt_time_prev = 0;
unsigned long wifi_time_prev = 0;
unsigned long rfid_time_prev = 0;
unsigned long check_time_prev = 0;

typedef struct m {                                          // Estrutuda de dados para salvar as configurações do servidor mqtt
  String server;
  String user;
  String password;
  int port;
  String subTopic;
} conf_mqtt;

conf_mqtt mqtt;
uint8_t GPIO_Pin = D2;

void setup() {
    initESP();
  /* ###### VARIAVEIS DO SISTEMA ########## 
  //secure_card[INDEX_ROOT][0] = "1234";

  delay(3000);*/

}

void loop() {
  if(_wifi_conectado != 1){
    conectar_wifi();
  } 
  if(_mqtt_conectado == 0){
    connect_mqtt();
  }
  client.loop();
  
  
  String v = lerRFID();
  if(!v.equals("")){
    msg("V: " + v);
    checkCardToOpen(v);
  }
  verif_conexao();
  timeClient.update();

  
  /* 
  //timeClient.update();
  
  if (Serial.available() > 0) {
    String m = Serial.readString();
    checkCommand(m);
  }
  ArduinoOTA.handle();
  */
  delay(50);
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
            auto ssid = doc["network"]["wifi"][0].as<char*>();
            auto wifi_password = doc["network"]["wifi"][1].as<char*>();

            auto mqtt_server = doc["network"]["mqtt"]["server"].as<char*>();
            auto mqtt_user = doc["network"]["mqtt"]["user"].as<char*>();
            auto mqtt_password = doc["network"]["mqtt"]["password"].as<char*>();
            auto mqtt_port = doc["network"]["mqtt"]["port"].as<int>();
            auto mqtt_topic = doc["network"]["mqtt"]["topic"].as<char*>();

            auto pin = doc["system"]["password"].as<char*>();

            Serial.println("\n\n**********************");
            Serial.println("NETWORK:");
            Serial.println("ssid: " + String(ssid) + "\nsenha: " + String(wifi_password));
            Serial.println("MQTT:");
            Serial.println("server: " + String(mqtt_server) + "\nuser: " + String(mqtt_user) + "\npassword: " + String(mqtt_password) + "\nport: " + String(mqtt_port) + "\ntopic: " + String(mqtt_topic));
            Serial.println("SYSTEM:");
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

    Serial.println("Configurando modo de trabalho do wifi...");
    // ###### WiFi CONFIG ######### 
    WiFi.mode(WIFI_STA);

    Serial.println("Configurando dados MQTT...");

    // ###### MQTT CONFIG ######### 
    client.setServer(mqtt.server.c_str(), mqtt.port);
    client.setCallback(callback);

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
    mqtt.server = String(doc["network"]["mqtt"]["server"].as<char*>());
    mqtt.user = String(doc["network"]["mqtt"]["user"].as<char*>());
    mqtt.password = String(doc["network"]["mqtt"]["password"].as<char*>());
    mqtt.port = doc["network"]["mqtt"]["port"].as<int>();
    mqtt.subTopic = doc["network"]["mqtt"]["topic"].as<char*>();

    PIN = doc["system"]["password"].as<char*>();
}

void publicar(String msg){
    client.publish((mqtt.subTopic + "/output").c_str(), msg.c_str());
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

void openDoor(){
  buzzer(1);
  digitalWrite(RELE, LOW);
  delay(500);
  digitalWrite(RELE, HIGH);
  msg("Publicando...");
  publicar("Aberto");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String m = "";
  for (int i = 0; i < length; i++) {
    m += (char)payload[i];
  }
  msg(m);
  checkCommand(m);
}



void connect_mqtt() {
    if (MQTT_ENABLE && _wifi_conectado){
        if(abs((millis() - mqtt_time_prev)) >= (WAIT_TIME_MQTT*1000)){
            mqtt_time_prev = millis();
            if(!client.connected()){
                msg("MQTT: CONECTANDO... [" + mqtt.server + ":" + mqtt.port + "]");
                String clientId = "ESP8266Client-";
                clientId += String(random(0xffff), HEX);
                int r = client.connect(clientId.c_str(), mqtt.user.c_str(), mqtt.password.c_str());
                //Serial.println(r ? "MQTT: FALHOU" : "MQTT: CONECTADO");
                if (r) {
                    client.publish((mqtt.subTopic + "/output").c_str(), "hello world");
                    client.subscribe(mqtt.subTopic.c_str());
                    _mqtt_conectado = 1;
                }
            }
        }
    }
}


void mostrarErroMqtt(int err){
    switch (err) {
        case MQTT_CONNECTION_TIMEOUT: msg("MQTT: SEM RESPOSTA"); break;
        case MQTT_CONNECTION_LOST: msg("MQTT: CONEXAO PERDIDA"); break;
        case MQTT_CONNECT_FAILED: msg("MQTT: CONEXAO FALHOU"); break;
        case MQTT_DISCONNECTED: msg("MQTT: DISCONECTADO."); break;
        case MQTT_CONNECTED: msg("MQTT: CONECTADO"); break;
        case MQTT_CONNECT_BAD_PROTOCOL: msg("MQTT: O SERVIDOR NAO SUPORTA VERSAO MQTT"); break;
        case MQTT_CONNECT_BAD_CLIENT_ID: msg("MQTT: SERVIDOR REJEITOU O ID DO CLIENTE"); break;
        case MQTT_CONNECT_UNAVAILABLE: msg("MQTT: SERVIDOR NAO DISPONIVEL PARA CONEXAO"); break;
        case MQTT_CONNECT_BAD_CREDENTIALS: msg("MQTT: EMAIL OU SENHA ERRADO"); break;
        case MQTT_CONNECT_UNAUTHORIZED: msg("MQTT: CLIENTE NAO AUTORIZADO"); break;
        default: break;
    }

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
    int mqtt_state = client.state();
    _mqtt_conectado = (mqtt_state == 0) ? 1 : 0;                 // Verifica se o MQTT ta conectado
    if (client.state() != 0) {
        mostrarErroMqtt(client.state());
    }
  }
}

void msg(String msg){
    Serial.println(diasHoras() + " -> " + msg);
}

String diasHoras(){
    String dias[7]={"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sabado"};
    return  (_wifi_conectado) ? dias[timeClient.getDay()] + ", " + timeClient.getFormattedTime() : "S/H";
}

void buzzer(int i){
  int q;
  for(q=0; q < i; q++){
    digitalWrite(BUZZ, HIGH);
    delay(100);
    digitalWrite(BUZZ, LOW);
    delay(100);
  }
}

void solicitarAbertura(String tag){
    String msg = "{\"type\":\"question\",\"timestamp\":\"" + diasHoras() + "\",\"data\":{\"cmd\":\"open\",\"tag\":\"" + tag + "\"}}";
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

void addNewUser(bool master, String user) {
  bool _add = true;
  String tagAtual;
  msg("NOVO CARTAO: INICIADO..");
  buzzer(1);
  int time_out = 0;                       // Tempo para aguardar enquanto não for inserido um novo cartão, se passar cancela a operação.
  int i = 0;
  do {
    tagAtual = lerRFID();              // Pega o cartão aproximado
    time_out++;
    if (time_out >= 40) {
      buzzer(2);
      msg("NOVO CARTAO: CANCELADO");
      return;
    }
    delay(250);
  } while (tagAtual.equals(""));
  if (master) {                                                                       // Se master for true, ele adiciona um novo master card
    secure_card[0][0] = tagAtual;
    if (user != NULL && !user.equals("")) {
      secure_card[0][1] = user;
    } else {
      secure_card[0][1] = "S/N";
    }
    msg("NOVO CARTAO: ADICIONADO[" + tagAtual+"]");
    buzzer(1);
  } else {
    for (i = 0; i < VECTOR_SIZE_MAX; i++) {                                           // Verifica se o cartão ja existe
      if (secure_card[i][0].equals(tagAtual)) { 
        msg("NOVO CARTAO: ERRO, CARTAO JA EXISTE.");
        buzzer(2);
        return;
      }
    }
    for (i = 1; i < VECTOR_SIZE_MAX; i++) {                                           // Verifica onde tem um espaço vazio para colocar um novo cartão, ignorando o primeiro
      if (secure_card[i][0].equals("")) {
        secure_card[i][0] = tagAtual;
        if (user != NULL && !user.equals("")) {
          secure_card[i][1] = user;
        } else {
          secure_card[i][1] = "S/N";
        }
        msg("NOVO CARTAO: ADICIONADO[" + tagAtual+"]");
        buzzer(1);
        break;
      }
    }
  }
  buzzer(1);
}


void checkCardToOpen(String tag){
    solicitarAbertura(tag);
}

void checkCommand(String mensagem) {
  if (!mensagem.equals("")) {
    mensagem.trim();
    openDoor();
  }
}
