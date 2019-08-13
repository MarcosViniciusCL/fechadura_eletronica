#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "WifiESP.h"
//#include "MQTT.h"
#include "RFID.h"
#include <EEPROM.h>
#include <PubSubClient.h>
#include <StringSplitter.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <ArduinoOTA.h>

#define SS_PIN D4
#define RST_PIN D2

#define VECTOR_SIZE_MAX 30
String secure_card[VECTOR_SIZE_MAX][2];
int counter_master_card=0;
int index_card=0;
int _look=0;

#define TRY_MAX 5                   // Tentativa maxima, para bloqueio, de usuario não cadastrado.
int _try=0;



#define LED_NOT_VERDE 2
#define LED_NOT_VERMELHO D3
#define RELE D1
#define BUZZER D3

// flags
bool __config_mode=false;                                   // Flag para entra no modo de configuração
bool __status=false;                                        // Flag usada para habilitar a exibição de status
bool __reset=false;                                         // Flag para um reset no microcontrolador
bool __only_master=false;
bool _ota_begin=false;

bool __save_new_card=false;                                 // Flag para habilitar função de adicionar novo cartão

typedef struct m {                                          // Estrutuda de dados para salvar as configurações do servidor mqtt  
  String server;
  String user;
  String password;
  int port;
  String subTopic;
} conf_mqtt; 

WiFiClient espClient;
PubSubClient client(espClient);
conf_mqtt mqtt;

WifiESP *wifi;                                              // Objeto da classe que controla o Wifi
RFID *rfid;                                                 // Objeto da classe que controla o leitor de RFID

char state;                                                 // O estado que se encontra o microcontrolador
int counter;                                                // Contador usado para saber o numero de execuçao
int counter_mqtt;                                           // Contador para conexão do mqtt
int web_server_timer;

os_timer_t tmr0;                                            // Intancia de time, controla a interrupção por time        
String tagAtual;                                            // Salva a ultima tag rfid lida 

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

ESP8266WebServer server(80);

void setup(){
  Serial.begin(115200);                                     // Configurando a velocidade da uart
  Serial.setTimeout(150);
  SPI.begin();                                              // Iniciando a interface SPI para usar o RFID    
  EEPROM.begin(4096);                                       // Iniciando o tamaho maximo da EEPROM

  //save_card();
  get_card();                                               // Carrega os cartões salvos na memoria.
      
  rfid = new RFID();                                        // Intancia a classe que controla o RFID
  rfid->init();                                             // Inicia o controlador
  
  wifi = new WifiESP(&state);                               // Criando objeto para controle do wifi
  wifi->setSSID("Cavalo de Troia.exe");                     // Configurando SSID
  wifi->setPassword("@n3tworking");                         // Configurando senha
  WiFi.begin(wifi->getSSID(), wifi->getPassword());
  wifi->loop();                                             // Primeira tentativa de conexão ao wifi
  //initOTA();                                                // Inicializada as configurações do upload de firmware por rede

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

  timeClient.begin();
  
  os_timer_setfn(&tmr0, interrupt_time, NULL);              //Indica ao Timer qual sera sua Sub rotina.
  os_timer_arm(&tmr0, 500, true);                           //Indica ao Timer seu Tempo em ms e se será repetido (loop = true)
  counter = 0;
  counter_mqtt = 0;
  counter_master_card = 0;
  web_server_timer=0;
  initServerWeb();
  buzzer(1000, 2);
}

void connect_mqtt(){
  if(WiFi.status() == WL_CONNECTED && counter_mqtt >= 10) {
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
      

/*
 * Funcão executada quando chega uma nova mensagem via mqtt
 */
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  check_command(msg);
}

void check_command(String msg){
  if(!msg.equals("")){
    msg.trim();
    StringSplitter *split;
    if (msg.equals("open_door")) {
      acionarRele("mqtt");
    } else if(msg.equals("add_new_user master")){
      add_new_user(true, "master");
    } else if(msg.substring(0, msg.indexOf(" ")).equals("add_new_user")){
      String s1 = msg.substring(msg.indexOf(" "));
      s1.trim(); s1.toUpperCase();
      add_new_user(false, s1);
    } else if(msg.substring(0, msg.indexOf(" ")).equals("change_wifi")){
      msg.replace("change_wifi", "");
      split = new StringSplitter(msg, ',', 2);
      String ssid = split->getItemAtIndex(0);
      Serial.println(ssid);
      ssid.trim();
      String pass = split->getItemAtIndex(1);
      pass.trim();
      Serial.println(pass);
      change_wifi(ssid, pass);
    } else if(msg.substring(0, msg.indexOf(" ")).equals("list_user")){
      list_user();
    } else if(msg.substring(0, msg.indexOf(" ")).equals("remove_user")){
      split = new StringSplitter(msg, ' ', 2);
      String tag = split->getItemAtIndex(1);
      tag.trim();
      remove_user(tag);
    } else if(msg.substring(0, msg.indexOf(" ")).equals("only_master")){
      String s1 = msg.substring(msg.indexOf(" "));
      s1.trim();
      s1.equals("enable") ? __only_master=true : __only_master=false;
      if(__only_master) printMsg("Sistema com acesso permitido apenas para master");
      else ("Sistema liberado para todos");
    } else if(msg.substring(0, msg.indexOf(" ")).equals("reset")){
      restart_esp();
    } else{
      printMsg("Erro command [cmd: " + msg + "]");
    }
  }
}




/**
 * Callback quando uma interrupção, por tempo, acontece
 */
void interrupt_time(void* z){
  counter += 1;
  if(counter%2 == 0){ // Acontece a cada 1 segundo
    _look > 0 ? _look-- : _look=0;
    web_server_timer > 0 ? web_server_timer-- : web_server_timer=0;
  }
  if(counter >= 40){ // 20 Segundos
    counter_master_card = 0;
    counter = 0;
  }
//  if(!__config_mode){
//    if(Serial.available() > 0){
//      String c = "";
//      while(Serial.available() > 0) c.concat((char)Serial.read());
//      if(c.equals("state\r\n")){
//        Serial.println((int)state);
//      }
//      if(c.equals("status\r\n")){
//        __status=true;
//      }
//      if(c.equals("reset\r\n")){
//        __reset=true;
//      }
//    }
//  }
  
}

/**
 * Limpa toda a EEPROM
 */
void clear_eeprom(){
  int i;
  printMsg("Limpando...");
  for(i = 0; i < 4096; i++){
    EEPROM.write(i, '\n');
    EEPROM.commit();  
    printMsg((100*i)/4096+"%");
  }
  printMsg("Limpeza concluída.");
}

String readSerial(){
  String c;
  while(Serial.available() <= 0){};
  c = Serial.readString();
  c.remove(c.length()-2, 2);
  return c;
}

String readSerialNoLock(){
  String c;
  if(Serial.available() > 0){
    c = Serial.readString();
    return c;
  }
  return "";
}

void restart_esp(){
  printMsg("Reiniciando sistema...");
  ESP.restart();
}

void status_all(){
  Serial.println("\n----------- STATUS -----------");
  Serial.print("\nWIFI: "); 
  WiFi.status() == WL_CONNECTED ? Serial.print("CONECTADO") : Serial.print("DESCONECTADO");
  Serial.print("\nMQtt: "); 
  client.connected() ? Serial.print("CONECTADO\n") : Serial.print("DESCONECTADO\n");
  Serial.print("\nAcess Point: "); 
  Serial.print(wifi->getSSID());
  __status=false;
}

void verif_conf(){
//  if(!isConnectWifi()){ //Se o wifi estiver deconectado
//    wifi->connectWifi();
//  }
  
  if(__status){
    status_all();
  }
  if(__reset){
    restart_esp();
  }
}

void acionarRele(String user){
  printMsg("Portão aberto por: "+ user +", "+ tagAtual);
  beepBuzzer(50);
  digitalWrite(RELE, 0);
  delay(1000);
  digitalWrite(RELE, 1);
}

/*
 * Verifica se o cartão inserido tem acesso
 */
int check_permition(String id){
   int i;
   if(__only_master){                                   //Verifica se o sistema ta bloqueado para outros cartões, caso esteja, só libera para o master
     if(secure_card[0][0].equals(id)){
      return 0; 
     }
     return -1;
   }
   if(counter_master_card >= 5) {   // Verifica se o cartão master foi inserido 5 vezes
    counter_master_card=0;
    mode_config();
    add_new_user(false, "");            // Adiciona um novo cartão, não master.
    return -1;
   }
   for(i = 0; i < VECTOR_SIZE_MAX; i++){
    if(secure_card[i][0].equals(id)){  // Se o cartão tiver no vetor, retorna verdadeiro
      if(i == 0){ 
        counter_master_card++;      // Conta a quantidade de vezes que o master passou, para acionar o modo de adicionar um novo cartão
      }
      return i; 
    }
   }
   return -1;
}

/*
 * Adiciona, ou remove, um cartão no bando de dados
 */
void add_new_user(bool master, String user){
  bool _add=true;
  printMsg("Adicinar/Remover cartão");
  buzzer(500, 3);
  int time_out=0;                         // Tempo para aguardar enquanto não for inserido um novo cartão, se passar cancela a operação.
  int i=0;
  do {
    tagAtual = rfid->loop();              // Pega o cartão aproximado
    time_out++;
    if(time_out >= 40){
      buzzer(100, 3);
      printMsg("Cancelando...");
      return;
    }
    delay(250);  
  } while(tagAtual.equals(""));
  if(master){                                                                         // Se master for true, ele adiciona um novo master card
    secure_card[0][0] = tagAtual;
    if(user != NULL && !user.equals("")){
      secure_card[0][1] = user;
    } else {
      secure_card[0][1] = "";
    }
  } else {
    for(i=0; i<VECTOR_SIZE_MAX; i++){                                                 // Verifica se o cartão ja existe
      if(secure_card[i][0].equals(tagAtual) && secure_card[0][0].equals(tagAtual)){   // Tratamento caso tente usar o cartão do master em outros usuarios
        printMsg("Não é possivel usar um cartão que pertence ao master.");
        _add=false;
      }
      if(secure_card[i][0].equals(tagAtual) && !secure_card[0][0].equals(tagAtual)){  // Caso o cartão exista e for diferente do master atual, o cartão é removido
        printMsg("Ja existe esse cartão, removendo...");
        remove_user(tagAtual);
        _add=false;                                                                   // Se um cartão for removido, não adiciona um novo abaixo 
      }
    }
    for (i=1; i<VECTOR_SIZE_MAX; i++){                                                // Verifica onde tem um espaço vazio para colocar um novo cartão, ignorando o primeiro
      if(secure_card[i][0].equals("")){
        index_card = i;
        break;
      }
    }
    if(_add){                                                                         // Se _add tive habilitado, um novo cartão é adicionado.
      secure_card[index_card][0] = tagAtual;
      if(user != NULL && !user.equals("")){
        secure_card[index_card][1] = user;
      } else {
        secure_card[index_card][1] = "";
      }
      printMsg("Um novo cartão adicionado: " + tagAtual);
    }
  }
  buzzer(800, 1);
  save_card();
}

void change_wifi(String ssid, String pass){
  wifi->setSSID(ssid);
  wifi->setPassword(pass);
  wifi->desconectar();  
}

void list_user(){
  String r = "";
  int i;
  for(i=0; i < VECTOR_SIZE_MAX; i++){
    if(!secure_card[i][0].equals("") && !secure_card[i][1].equals("")){
      r = (i+":" + secure_card[i][1] + ", " + secure_card[i][0]);
      printMsg(r);
    }
  }
  
}

void remove_user(String tag){
  int i;
  for(i=0; i < VECTOR_SIZE_MAX; i++){
    if(secure_card[i][0].equals(tag)){
      secure_card[i][0] = "";
      secure_card[i][1] = "";
      printMsg("Usuário tag \'" + tag + "\' removido");
      save_card();
      return;
    }
  }
  printMsg("Não existe: " + tag);
}

void get_card(){
  EEPROM.get(0, secure_card);
}
void save_card(){
  EEPROM.put(0, secure_card);
  EEPROM.commit();
}
void loop(){
    ArduinoOTA.handle();
    OTABegin();                                               // Executado apenas uma vez assim que estiver conectado a uma rede wifi
    if(!wifi->loop() && web_server_timer == 0){               // Verifica se o wifi ainda permanece conectado
      mode_config();                                           // Se não for possivel encotrar uma rede wifi, habilitar modo configuração ligando o AP
    }
    if(_look == 0){                                           // Se não tiver bloqueado entra na condição
      tagAtual = rfid->loop();                                // Lê a tag rfid
      if(!tagAtual.equals("")){
        int r = check_permition(tagAtual);                               
        if(r != -1){                        // Verifica se tem acesso no sistema 
          acionarRele(secure_card[r][1]);
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
    if (!client.connected()) {                                // Verifica se o cliente mqtt ainda esta conectado
      connect_mqtt();
    }
    timeClient.update();
    client.loop();
    server.handleClient();
    if(web_server_timer == 0 && WiFi.getMode() == WIFI_AP_STA){
      end_mode_config();
      printMsg("Servidor web parado");
    }
    check_command(readSerialNoLock());
    verif_conf();                                             // Verifica o estado de algumas configurações
}

void printMsg(String s){
  String time_c = "";
  if(WiFi.status() == WL_CONNECTED){
    time_c = timeClient.getDayInText()+", "+ timeClient.getFormattedTime();
  }
  Serial.println(time_c +" - "+ s);
  client.publish((mqtt.subTopic + "/log").c_str(),(time_c +" - "+ s).c_str());
}
void mode_config(){
  printMsg("Modo configuração habilitado");
  web_server_timer = 900; // 15 min
  WiFi.softAP("FECHADURA", "87654321");
  server.begin();
}
void end_mode_config(){
  server.close();
  server.stop();
  WiFi.mode(WIFI_STA);
}

void initServerWeb(){
    server.onNotFound(handleNotFound);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  check_command(server.arg(0));
}


// ################################################# BUZZER ############################################

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

// ################################### Atualizações OTA #######################################
void initOTA(){
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword("+8socram");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    printMsg("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    printMsg("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    String msg = "Progress: " + progress;
    msg += "/" + total;
    printMsg(msg);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      printMsg("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      printMsg("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      printMsg("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      printMsg("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      printMsg("End Failed");
    }
  });
  OTABegin();
}

void OTABegin(){
  if(WiFi.status() == WL_CONNECTED && !_ota_begin){
    ArduinoOTA.begin();
    _ota_begin=true;
  }
}
