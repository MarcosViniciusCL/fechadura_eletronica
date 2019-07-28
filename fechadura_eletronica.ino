#include <SPI.h>
#include <ESP8266WiFi.h>
#include "WifiESP.h"
//#include "MQTT.h"
#include "RFID.h"
#include <EEPROM.h>
#include <PubSubClient.h>

#define SS_PIN D4
#define RST_PIN D2

#define VECTOR_SIZE_MAX 30
String secure_card[VECTOR_SIZE_MAX];
int counter_master_card=0;
int index_card=0;
int _look=0;

#define TRY_MAX 5
int _try=0;



#define LED_NOT_VERDE 2
#define LED_NOT_VERMELHO D3
#define RELE D1
#define BUZZER D3

// flags
bool __config_mode=false;                                   // Flag para entra no modo de configuração
bool __status=false;                                        // Flag usada para habilitar a exibição de status
bool __reset=false;                                         // Flag para um reset no microcontrolador

bool __save_new_card=false;

typedef struct m {
  String server;
  String user;
  String password;
  int port;
  String subTopic;
} conf_mqtt; 

WiFiClientSecure espClient;
PubSubClient client(espClient);
conf_mqtt mqtt;

WifiESP *wifi;
RFID *rfid;

char state;                                                 // O estado que se encontra o microcontrolador
int counter;
int counter_mqtt;

os_timer_t tmr0;
String tagAtual;



void setup(){
  Serial.begin(115200);                                     //Configurando a velocidade da uart
  SPI.begin();                                              //Iniciando a interface SPI para usar o RFID    
  EEPROM.begin(4096);
  int i = 0;

  get_card();
  rfid = new RFID();
  rfid->init();
  
  wifi = new WifiESP(&state);                               //Criando objeto para controle do wifi
  //wifi->loadMemory();
  wifi->setSSID("Cavalo de Troia.exe");                                    //Configurando SSID
  wifi->setPassword("@n3tworking");                         //Configurando senha
  WiFi.begin(wifi->getSSID(), wifi->getPassword());
  //wifi->saveMemory();

  pinMode(LED_NOT_VERDE, OUTPUT);
  pinMode(RELE, OUTPUT);                                    // Informando que o pino do rele é uma saída
  pinMode(BUZZER, OUTPUT);
  digitalWrite(RELE, 1);                                    // Escrevendo nivel logico alto no pino do rele, o rele é acionado com nivel logico baixo

  mqtt.server = "postman.cloudmqtt.com";
  mqtt.user = "jsxgzfyb";
  mqtt.password = "Gb5a2cgqb_K6";
  mqtt.port = 14157;
  mqtt.subTopic = "/galeria/portao/esp8266";
  client.setServer(mqtt.server.c_str(), mqtt.port);
  client.setCallback(callback);
  //mqtt.saveMemory();
  //mqtt.loadMemory();
  
  
  os_timer_setfn(&tmr0, interrupt_time, NULL);              //Indica ao Timer qual sera sua Sub rotina.
  os_timer_arm(&tmr0, 500, true);                           //Indica ao Timer seu Tempo em ms e se será repetido (loop = true)
  counter = 0;
  counter_mqtt = 0;
  counter_master_card = 0;
  buzzer(1000, 2);
}

void connect_mqtt(){
  const char* finderprint = "5e 5d c0 cd a9 6d 68 c1 09 a4 e3 96 27 b0 33 de c4 36 cf 5b";
    if(WiFi.status() == WL_CONNECTED && counter_mqtt >= 1000) {
        Serial.println("\nTentando conectar MQtt");
        if (client.connect("ESP8266Client", mqtt.user.c_str(), mqtt.password.c_str())) {
          Serial.println("\nConectado MQtt");
          client.subscribe(mqtt.subTopic.c_str());
        }
        counter_mqtt = 0;
    }
    counter_mqtt++;
    if(counter_mqtt > 1000) counter_mqtt = 0;
}
      

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  // Switch on the LED if an 1 was received as first character
  if (msg.equals("1")) {
    acionarRele("mqtt");
  } else if(msg.equals("add_new_card")){
    add_new_card(false);
  } else if(msg.equals("add_new_card master")){
    add_new_card(true);
  }
}

void beepBuzzer(int delayMs){
  digitalWrite(BUZZER, 1);
  delay(delayMs);
  digitalWrite(BUZZER, 0);
}

void buzzer(int delay_t, int co_unt){
  int i = 0;
  for(i = 0; i < co_unt; i++){
    delay(delay_t);
    digitalWrite(BUZZER, !digitalRead(BUZZER));
  }
  digitalWrite(BUZZER, 0);
}

void beepBuzzerErro(int delayMs){
  digitalWrite(BUZZER, 1);
  delay(delayMs);
  digitalWrite(BUZZER, 0);
  delay(delayMs);
  digitalWrite(BUZZER, 1);
  delay(delayMs);
  digitalWrite(BUZZER, 0);
  delay(delayMs);
  digitalWrite(BUZZER, 1);
  delay(delayMs);
  digitalWrite(BUZZER, 0);
}


void notificacao_led(){
  counter += 1;
  if(counter%2 == 0){ // Acontece a cada 1 segundo
    _look > 0 ? _look-- : _look=0;
  }
  if(counter >= 40){ // 20 Segundos
    counter_master_card = 0;
    counter = 0;
  }
}

void interrupt_time(void* z){
  if(!__config_mode){
    if(Serial.available() > 0){
      String c = "";
      while(Serial.available() > 0) c.concat((char)Serial.read());
      if(c.equals("config\r\n")){
        __config_mode=true;
      }
      if(c.equals("state\r\n")){
        Serial.println((int)state);
      }
      if(c.equals("status\r\n")){
        __status=true;
      }
      if(c.equals("reset\r\n")){
        __reset=true;
      }
    }
    notificacao_led();
  }
  
}

void clear_eeprom(){
  int i;
  printMsg("Limpando...");
  for(i = 0; i < 4096; i++){
    EEPROM.write(i, '\n');
    EEPROM.commit();  
  }
  printMsg("Concluido.");
}

String readSerial(){
  String c;
  while(Serial.available() <= 0){};
  c = Serial.readString();
  c.remove(c.length()-2, 2);
  return c;
}

void reiniciar(){
  printMsg("Reiniciando...");
  //resetEsp();
}

void config_all(){
  String c;
  Serial.println("\n\n************************************************************");
  Serial.println("******************* MODO CONFIGURAÇÃO **********************");
  Serial.println("******* OBS: SISTEMA FICA PARADO NO MODO CONFIG ************");
  Serial.println("DIGITE UMA DAS OPÇÕES");
  Serial.println("\nwifi: Entrar nas configurações do wifi");
  Serial.println("mqtt: Entrar nas configurações do serviço MQTT");
  c = readSerial();
  if(c.equals("wifi")){
    config_wifi();
  }
  if(c.equals("mqtt")){
    config_mqtt();
  }
  __config_mode=false;
}

void config_wifi(){
  Serial.print("\nWIFI CONFIG\n");
  Serial.print("SSID: " + String(wifi->getSSID()));
  Serial.print("\nPASSWORD: " + String(wifi->getPassword()));
  Serial.print("\nA - Alterar configurações\n");
  Serial.print("S - Sair\n");
  if(readSerial().equals("A")){
    Serial.print("Digite as opções\n");
    Serial.print("SSID: ");
    wifi->setSSID(readSerial());
    Serial.print(wifi->getSSID());
    Serial.print("\nPASSWORD: ");
    wifi->setPassword(readSerial());
    Serial.print(wifi->getPassword());
    Serial.print("\n\nConfigurações salvas!\n");
    wifi->desconectar();
    wifi->saveMemory();
    //saveConfigWifi(); //Tem que desconectar e conectar novamente no wifi
  }
}
void config_mqtt(){
  Serial.print("\nMQTT CONFIG\n");
  Serial.print("SERVER: " + String(mqtt.server));
  Serial.print("\nPORT: " + String(mqtt.port));
  Serial.print("\nUSER: " + String(mqtt.user));
  Serial.print("\nPASSWORD: " + String(mqtt.password));
  Serial.print("\nA - Alterar configurações\n");
  Serial.print("S - Sair\n");
  if(readSerial().equals("A")){
    Serial.print("Digite as opções\n");
    Serial.print("\nSERVER: ");
    mqtt.server = readSerial();
    Serial.print(mqtt.server);

    Serial.print("\nPORT: ");
    mqtt.port = readSerial().toInt();
    Serial.print(mqtt.port);
    
    Serial.print("\nUSER: ");
    mqtt.user = readSerial();
    Serial.print(mqtt.user);
    
    Serial.print("\nPASSWORD: ");
    mqtt.password = readSerial();
    Serial.print(mqtt.password);

    Serial.print("\nTOPIC: ");
    mqtt.subTopic = readSerial();
    Serial.print(mqtt.subTopic);
    
    Serial.print("\n\nConfigurações salvas!\n");
    //saveConfigMqtt();
    //conecte_broker(); //Disconectar broker e conectar novamente
  } 
}

void status_all(){
  Serial.println("\n----------- STATUS -----------");
  Serial.print("\nWIFI: "); 
  WiFi.status() == WL_CONNECTED ? Serial.print("CONECTADO") : Serial.print("DESCONECTADO");
  Serial.print("\nMQtt: "); 
  client.connected() ? Serial.print("CONECTADO\n") : Serial.print("DESCONECTADO\n");
  Serial.print("\nAcess Point: "); 
  Serial.print(wifi->getSSID());
  Serial.print(wifi->getStatus());
  __status=false;
}

void verif_conf(){
//  if(!isConnectWifi()){ //Se o wifi estiver deconectado
//    wifi->connectWifi();
//  }
  
  if(__config_mode){
    config_all();
  }
  if(__status){
    status_all();
  }
  if(__reset){
    reiniciar();
  }
}

void acionarRele(String user){
  client.publish((mqtt.subTopic + "/log").c_str(), ("Portão aberto por: " + user).c_str());
  beepBuzzer(50);
  digitalWrite(RELE, 0);
  delay(1000);
  digitalWrite(RELE, 1);
}

bool check_permition(String id){
   int i;
   if(counter_master_card >= 5) {
    add_new_card(false);            // Adiciona um novo cartão, não master.
    return false;
   }
   for(i = 0; i < VECTOR_SIZE_MAX; i++){
    if(secure_card[i].equals(id)){
      if(i == 0){ 
        counter_master_card++;
      }
      return true; 
    }
   }
   return false;
}

void add_new_card(bool master){
  bool _add=true;
  printMsg("Adicinar/Remover cartão");
  buzzer(500, 3);
  int time_out=0;
  int i=0;
  do {
    tagAtual = rfid->loop();
    time_out++;
    if(time_out >= 40){
      buzzer(100, 3);
      printMsg("Cancelando...");
      return;
    }
    delay(250);  
  } while(tagAtual.equals(""));
  if(master){                         // Se master for true ele adiciona um novo master card
    secure_card[i] = tagAtual;
  } else {
    for(i=0; i<VECTOR_SIZE_MAX; i++){
      if(secure_card[i].equals(tagAtual) && !secure_card[0].equals(tagAtual)){
        printMsg("Ja existe esse cartão, removendo...");
        secure_card[i] = "";
        _add=false;
      }
    }
    for (i=0; i<VECTOR_SIZE_MAX; i++){
      if(secure_card[i].equals("")){
        index_card = i;
        break;
      }
    }
    if(_add){
      secure_card[index_card] = tagAtual;
      printMsg("Um novo cartão adicionado: " + tagAtual);
    }
  }
  counter_master_card=0;
  buzzer(800, 1);
  save_card();
}

void get_card(){
  EEPROM.get(0, secure_card);
}
void save_card(){
  EEPROM.put(0, secure_card);
  EEPROM.commit();
}
void loop(){
    wifi->loop();
    if(_look == 0){
      tagAtual = rfid->loop();
      if(!tagAtual.equals("")){
        if(check_permition(tagAtual)){
          acionarRele(tagAtual);
          _try=0;
        } else {
          printMsg("Tentativa de abrir por: " + tagAtual);
          beepBuzzer(300);
          if(_try >= TRY_MAX){
            printMsg("SISTEMA BLOQUEADO");
            _look=20;
            _try=0;
          }
          _try++;
        }
      }
    }
    if (!client.connected()) {
      connect_mqtt();
    }
    client.loop();
    verif_conf();
}

void printMsg(String s){
  Serial.println(s);
  client.publish((mqtt.subTopic + "/log").c_str(), s.c_str());
}
