#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <user_interface.h>

#define DEBUG
#define LED_NOT_VERDE 2
#define LED_NOT_VERMELHO 3

//informações do broker MQTT - Verifique as informações geradas pelo CloudMQTT

#define BOOTING  0x00 // Boot
#define CONNECTING_WIFI  0x01 // Conectando ao wifi
#define CONNECTED_WIFI   0x02 // Conectado ao wifi
#define CONNECTING_MQTT  0x03 // Conectando ao broker MQtt
#define CONNECTED_MQTT   0x04 // Conectado ao wifi
#define READY  0x05 // Pronto para uso
#define BRIDGE  0x06 // Estado NULL, entre um estado e outro



//Erros
#define E_S10 0x10 // Rede wifi não encontrada
#define E_S20 0x20 // Sem conexão com Broker


os_timer_t tmr0;
int counter = 0;
char state = 0x00; //Variavel para estado do sistema

// flags
bool __config_mode=false;
bool __status=false;

typedef struct ConfigWifi {
  String ssid;
  String password;
} ConfigWifi;

typedef struct {
  String server;
  int port;
  String user;
  String password;
  String topicSub;
} ConfigMqtt;

ConfigWifi configWifi;
ConfigMqtt configMqtt;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
 // EEPROM.begin(512);
  // ------- Carregando configurações do wifi 
  getConfigMqtt();
  //configWifi.ssid = "NOTE";
  //configWifi.password = "@n3tworking";
  /*configMqtt.server = "postman.cloudmqtt.com";   
  configMqtt.user = "jsxgzfyb";              
  configMqtt.password = "kbnsDu-ZSXlS";      
  configMqtt.port = 14157;                    
  configMqtt.topicSub ="almiro_galeria/atuadores/porta_escada";            //tópico que sera ass*/
   
  os_timer_setfn(&tmr0, notif_usuario, NULL); //Indica ao Timer qual sera sua Sub rotina.
  os_timer_arm(&tmr0, 100, true);  //Inidica ao Timer seu Tempo em mS e se sera repetido ou apenas uma vez (loop = true)
                                     //Neste caso, queremos que o processo seja repetido, entao usaremos TRUE.
  //state = getCurState();
  Serial.begin(115200);
  pinMode(LED_NOT_VERDE, OUTPUT);
  conecte_wifi();
  client.setServer(configMqtt.server.c_str(), configMqtt.port);
  client.setCallback(callback);
  conecte_broker();
  //subscreve no tópico
  client.subscribe(configMqtt.topicSub.c_str());
  //setCurState(READY);
}

void conecte_wifi(){
  if(getCurState() != CONNECTING_WIFI){
    WiFi.begin(configWifi.ssid, configWifi.password);
    Serial.print(configWifi.ssid);
    #ifdef DEBUG
        Serial.println("\nConectando ao WiFi..");  
    #endif
  }
  setCurState(CONNECTING_WIFI);
  if (WiFi.status() != WL_CONNECTED) {
    
  }
  if (WiFi.status() == WL_CONNECTED) {
    #ifdef DEBUG
    Serial.println("\nConectado na rede WiFi " + configWifi.ssid);
    #endif
  }
  delay(100);
 
}

void conecte_broker(){
  if(WiFi.status() == WL_CONNECTED) {
    setCurState(CONNECTING_MQTT);
    if (!client.connected()) {
      if (!isConnectWifi()) conecte_wifi();
      #ifdef DEBUG
      Serial.println("\nConectando ao Broker MQTT...");
      #endif
   
      if (client.connect("ESP8266Client", configMqtt.user.c_str(), configMqtt.password.c_str())) {
        setCurState(CONNECTED_MQTT);
        #ifdef DEBUG
        Serial.println("\nConectado");  
        #endif
   
      } else {
        #ifdef DEBUG 
        Serial.print("falha estado  ");
        Serial.print(client.state());
        #endif
        delay(2000);
      }
    }
  }
  //setCurState(BRIDGE);
}
 
void callback(char* topic, byte* payload, unsigned int length) {

  //armazena msg recebida em uma sring
  payload[length] = '\0';
  String strMSG = String((char*)payload);

  #ifdef DEBUG
  Serial.print("Mensagem chegou do tópico: ");
  Serial.println(topic);
  Serial.print("Mensagem:");
  Serial.print(strMSG);
  Serial.println();
  Serial.println("-----------------------");
  #endif

  //aciona saída conforme msg recebida 
  if (strMSG == "1"){         //se msg "1"
     digitalWrite(LED_NOT_VERDE, LOW);  //coloca saída em LOW para ligar a Lampada - > o módulo RELE usado tem acionamento invertido. Se necessário ajuste para o seu modulo
  }else if (strMSG == "0"){   //se msg "0"
     digitalWrite(LED_NOT_VERDE, HIGH);   //coloca saída em HIGH para desligar a Lampada - > o módulo RELE usado tem acionamento invertido. Se necessário ajuste para o seu modulo
  }
 
}

//função pra reconectar ao servido MQTT
void reconect() {
  if(WiFi.status() == WL_CONNECTED) {
    //Enquanto estiver desconectado
    setCurState(CONNECTING_MQTT);
    if (!client.connected()) {
      if (!isConnectWifi()){ conecte_wifi(); setCurState(CONNECTING_MQTT);}
      #ifdef DEBUG
      Serial.print("Tentando conectar ao servidor MQTT");
      #endif
       
      bool conectado = strlen(configMqtt.user.c_str()) > 0 ? client.connect("ESP8266Client", configMqtt.user.c_str(), configMqtt.password.c_str()) : client.connect("ESP8266Client");
  
      if(conectado) {
        #ifdef DEBUG
        Serial.println("\nConectado!");
        #endif
        //subscreve no tópico
        client.subscribe(configMqtt.topicSub.c_str(), 1); //nivel de qualidade: QoS 1
      } else {
        #ifdef DEBUG
        Serial.print("Falha durante a conexão.Code: ");
        Serial.println( String(client.state()).c_str());
        #endif
        //Aguarda 3 segundos 
        delay(3000);
      }
    }
    setCurState(READY);
  }
  
}

void verif_prim_execucao(){
  EEPROM.read(0);
}

void notif_usuario(void* z){
  if(!__config_mode){
    counter += 1;
    switch(state){
        case(CONNECTING_WIFI):
          if(counter >= 1){ // 100 ms
              counter = 0;
              digitalWrite(LED_NOT_VERDE, !digitalRead(LED_NOT_VERDE));
          }
          break;
        case(CONNECTING_MQTT):
          if(counter >= 8){ // 1 segundo
            counter = 0;
            digitalWrite(LED_NOT_VERDE, !digitalRead(LED_NOT_VERDE));
          }
          break;
        case(READY):
          if(counter >= 30){ // 3 segundos
            counter = 0;
            digitalWrite(LED_NOT_VERDE, !digitalRead(LED_NOT_VERDE));
          }
          break;
        case(BRIDGE):
            digitalWrite(LED_NOT_VERDE, 1);
          break;
        default:
          break;
    }
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
    }
  } else {
    digitalWrite(LED_NOT_VERDE, 0);
  }
}

char getCurState(){
  return state;
}

char setCurState(char value){
  state = value;
}


// Apaga toda a EEPROM para um reset de dados. Comando deve ser recebido via MQTT.
void resetAll(){
  int l = EEPROM.length();
  int i = 0;
  for(i=0; i<l; i++){
    EEPROM.write(i, 0x00);
  }
}

//Verifica se o ESP ainda se encontra conectado ao WIFI
bool isConnectWifi(){
  int v = WiFi.status();
  return !(v == WL_DISCONNECTED || v == WL_CONNECTION_LOST || v == WL_CONNECT_FAILED || v == 1 );
}

void openPort(){
  
}

void config_all(){
  String c;
  Serial.println("\n\n************************************************************");
  Serial.println("******************* MODO CONFIGURAÇÃO **********************");
  Serial.println("************************************************************");
  Serial.println("DIGITE UMA DAS OPÇÕES");
  Serial.println("\nconfig_wifi: Entrar nas configurações do wifi");
  Serial.println("\nconfig_mqtt: Entrar nas configurações do serviço MQTT");
  c = readSerial();
  if(c.equals("config_wifi")){
    config_wifi();
  }
  if(c.equals("config_mqtt")){
    config_mqtt();
  }
  __config_mode=false;
}

void config_wifi(){
  Serial.print("\nWIFI CONFIG\n");
  Serial.print("SSID: " + String(configWifi.ssid));
  Serial.print("\nPASSWORD: " + String(configWifi.password));
  Serial.print("\nA - Alterar configurações\n");
  Serial.print("S - Sair\n");
  if(readSerial().equals("A")){
    Serial.print("Digite as opções\n");
    Serial.print("SSID: ");
    configWifi.ssid = readSerial();
    Serial.print(configWifi.ssid);
    Serial.print("\nPASSWORD: ");
    configWifi.password = readSerial();
    Serial.print(configWifi.password);
    Serial.print("\n\nConfigurações salvas!\n");
    //saveConfigWifi();
    conecte_wifi();
  }
}
void config_mqtt(){
  Serial.print("\nMQTT CONFIG\n");
  Serial.print("SERVER: " + String(configMqtt.server));
  Serial.print("\nPORT: " + String(configMqtt.port));
  Serial.print("\nUSER: " + String(configMqtt.user));
  Serial.print("\nPASSWORD: " + String(configMqtt.password));
  Serial.print("\nA - Alterar configurações\n");
  Serial.print("S - Sair\n");
  if(readSerial().equals("A")){
    Serial.print("Digite as opções\n");
    Serial.print("\nSERVER: ");
    configMqtt.server = readSerial();
    Serial.print(configMqtt.server);

    Serial.print("\nPORT: ");
    configMqtt.port = readSerial().toInt();
    Serial.print(configMqtt.port);
    
    Serial.print("\nUSER: ");
    configMqtt.user = readSerial();
    Serial.print(configMqtt.user);
    
    Serial.print("\nPASSWORD: ");
    configMqtt.password = readSerial();
    Serial.print(configMqtt.password);

    Serial.print("\nTOPIC: ");
    configMqtt.topicSub = readSerial();
    Serial.print(configMqtt.topicSub);
    
    Serial.print("\n\nConfigurações salvas!\n");
    saveConfigMqtt();
    conecte_broker();
  } 
}

void status_all(){
  Serial.println("\n----------- STATUS -----------");
  Serial.print("\nWIFI: "); 
  isConnectWifi() ? Serial.print("CONECTADO") : Serial.print("DESCONECTADO");
  Serial.print("\nMQtt: "); 
  client.connected() ? Serial.print("CONECTADO\n") : Serial.print("DESCONECTADO\n");
  Serial.print("\nAcess Point: "); 
  Serial.print((char*)WiFi.BSSID());
  __status=false;
}
void saveConfigMqtt(){
  EEPROM.put(0, configMqtt);
}


void getConfigMqtt(){
  EEPROM.get(0, configMqtt);
}

String readSerial(){
  String c;
  while(Serial.available() <= 0){};
  //while(Serial.available() > 0) c.concat((char)Serial.read());
  c = Serial.readString();
  c.remove(c.length()-2, 2);
  return c;
}

void loop() {
  if(!isConnectWifi()){ //Se o wifi estiver deconectado
    conecte_wifi();
  }
  if (!client.connected()) { // Se a conexão com o broker MQtt tive caído
    reconect();
  }
  if(__config_mode){
    config_all();
  }
  if(__status){
    status_all();
  }
  client.loop();
  
}
