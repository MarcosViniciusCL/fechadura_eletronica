
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
#include <AESLib.h>

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

char MQTT_ENABLE = 1;

#define VECTOR_SIZE_MAX 3
String secure_card[VECTOR_SIZE_MAX+1][2];
int INDEX_ROOT = VECTOR_SIZE_MAX + 1;

String PIN = "";

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key; 

WiFiClient espClient;
WiFiClient socket;
PubSubClient client(espClient);

StaticJsonDocument<200> doc;

//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

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
uint8_t pin_int = D2;

void setup() {
  Serial.begin(115200);
  pinMode(RELE, OUTPUT);
  pinMode(BUZZ, OUTPUT);
  digitalWrite(RELE, HIGH);
  digitalWrite(BUZZ, LOW);

  

  /* ###### SPIFFS ###### */
  if(!SPIFFS.begin()){
    Serial.println("\nErro ao abrir o sistema de arquivos");
  }
  getPin();
  getCards();
  
  /* ##### RFID CONFIG #### */
  SPI.begin();
  rfid.PCD_Init();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  /* ###### WiFi CONFIG ######### */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  /* ###### MQTT CONFIG ######### */
  mqtt.server = "postman.cloudmqtt.com";
  mqtt.user = "jsxgzfyb";
  mqtt.password = "Gb5a2cgqb_K6";
  mqtt.port = 14157;
  mqtt.subTopic = "/galeria/portao/esp8266";
  client.setServer(mqtt.server.c_str(), mqtt.port);
  client.setCallback(callback);

  /* ###### VARIAVEIS DO SISTEMA ########## */
  //secure_card[INDEX_ROOT][0] = "1234";

  delay(3000);
  msg("\nSISTEMA INICIADO");
}

void loop() {
  if(_wifi_conectado != 1){
    conectar_wifi();
  }
  if(_mqtt_conectado != 1){
    connect_mqtt();
  }
  
  String v = lerRFID();
  if(!v.equals("")){
    checkCardToOpen(v);
  }

  client.loop();
  //timeClient.update();
  verif_recurso();
  if (Serial.available() > 0) {
    String m = Serial.readString();
    checkCommand(m);
  }
  ArduinoOTA.handle();
  
  //delay(50);
}

void setupOta(){
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp_fechadura");

  // No authentication by default
  //ArduinoOTA.setPassword("mv4016");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  msg("Atualização OTA: ON");
  msg("IP: " + WiFi.localIP());
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
      Serial.println(F("Your tag is not of type MIFARE Classic."));
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
}

void callback(char* topic, byte* payload, unsigned int length) {
  String m = "";
  for (int i = 0; i < length; i++) {
    m += (char)payload[i];
  }
  msg(m);
  checkCommand(m);
}

void connect_serve(){
  if(socket.connect(server, porta)){
    msg("Conectado " + server);
    socket.println("Oi bb");
  }
}

void enable_mqtt(String r){
    MQTT_ENABLE = (r.equals("true"))  ? 1 : MQTT_ENABLE;
    MQTT_ENABLE = (r.equals("false")) ? 0 : MQTT_ENABLE;
}

void connect_mqtt() {
  if(MQTT_ENABLE){
    _mqtt_conectado = (client.connected()) ? 1 : 0;
    if(_mqtt_conectado){
      msg("MQTT: CONECTADO");
      return;
    }
    if((millis() - mqtt_time_prev) >= (WAIT_TIME_MQTT*1000)){
      connect_serve();
      mqtt_time_prev = millis();
      if (_wifi_conectado == 1) {
        msg("MQTT: CONECTANDO...");
        int r = client.connect("ESP8266Client", mqtt.user.c_str(), mqtt.password.c_str());
        if (r) {
          client.subscribe(mqtt.subTopic.c_str());
          _mqtt_conectado = 1;
          msg("MQTT: CONECTADO");
        }
      }
   }
  }
}

void conectar_wifi(){
  _wifi_conectado = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  if (_wifi_conectado == 1){
    msg("WIFI: CONECTADO ["+WiFi.SSID()+"]");
    setupOta();
    return;
  }
  if((millis()-wifi_time_prev) >= (WAIT_TIME_WIFI*1000)){
    wifi_time_prev = millis();
    WiFi.begin(ssid.c_str(), password.c_str());
    msg("WIFI: CONECTANDO... ["+WiFi.SSID()+"]");
  }
}

void verif_recurso(){
  if((millis() - check_time_prev) >= (CHECK_TIME*1000)){
    check_time_prev = millis();
    _wifi_conectado = (WiFi.status() == WL_CONNECTED) ? 1 : 0;      // Verifica se tem conexão com a internet
    _mqtt_conectado = (client.connected()) ? 1 : 0;                 // Verifica se o MQTT ta conectado
  }
}

void msg(String msg){
  String time_c = ""; //(_wifi_conectado) ? timeClient.getDayInText() + ", " + timeClient.getFormattedTime() : "S/H";
  Serial.println(" " + msg);
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

/* ################################### CONFIGURAÇÃO DO SISTEMA E USUARIOS ##########################################*/

bool _root = false;

/**
 * Login para usuario root na maquina. Sem a autenticação, não é permitido alterar configuração do sistema.
 */
void login(String pin_test){
  _root = (pin_test.equals(PIN)) ? true : false;
  msg((_root) ? "LOGIN: SUCESSO" : "LOGIN: FALHA");
}

void logout(){
  _root = false;
  msg("Logout");
}

void changePin(String pin_new){
  if(pin_new.length() == 4){
    PIN = pin_new;
    savePin();
    _root = false;
    msg("Senha salva. Entre novamente");
  } else {
    msg("Nova senha não contém 4 digitos.");
  }
}


void savePin(){
  File rFile = SPIFFS.open("/PIN","w+"); 
  if(!rFile){
    msg("Erro ao abrir arquivo!");
  } else {
    rFile.println(PIN);
  }
  rFile.close();
}

void getPin(){
  File rFile = SPIFFS.open("/PIN","r");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  String content = rFile.readStringUntil('\r'); //desconsidera '\r\n'
  rFile.close();
  content.trim();
  PIN = content;
}

bool saveCards(){
  int i;
  String str = "";
  for(i=0; i < VECTOR_SIZE_MAX; i++){
    if(!secure_card[i][0].equals("")){
      str += secure_card[i][0]+":"+secure_card[i][1]+","; 
    }
  }
  File rFile = SPIFFS.open("/CARDS","w+"); 
  if(!rFile){
    msg("Erro ao abrir arquivo!");
    return false;
  } else {
    rFile.println(str);
  }
  rFile.close();
  return true;
}

void getCards(){
  File rFile = SPIFFS.open("/CARDS","r");
  if (!rFile) {
    Serial.println("[ERRO] NAO FOI POSSIVEL CARREGAR OS CARTOES SALVOS");
  }
  String content = rFile.readStringUntil('\r'); //desconsidera '\r\n'
  rFile.close();
  content.trim();

  int i;
  int j=0;
  String buff = "";
  for(i=0; i < content.length(); i++){
    if(content[i] == ','){
      StringSplitter *split = new StringSplitter(buff, ':', 2);
      String tag = split->getItemAtIndex(0);
      String nome = split->getItemAtIndex(1);
      secure_card[j][0] = tag;
      secure_card[j][1] = nome;
      buff = "";
      j++;
      i++;
    }
    buff += content[i];
  }
}



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
  saveCards();
}


void removeUser(String tag) {
  int i;
  for (i = 0; i < VECTOR_SIZE_MAX; i++) {
    if (secure_card[i][0].equals(tag)) {
      secure_card[i][0] = "";
      secure_card[i][1] = "";
      msg("USUARIO REMOVIDO: " + tag);
      saveCards();
      return;
    }
  }
  msg("Não existe: " + tag);
}


void changeWifi(String s, String p) {
  ssid = s;
  password = p;
  WiFi.disconnect();
}

void listUser() {
  String r = "";
  int i;
  for (i = 0; i < VECTOR_SIZE_MAX; i++) {
    if (!secure_card[i][0].equals("")) {
      r = (secure_card[i][1] + ", " + secure_card[i][0]);
      msg(r);
    }
  }
  if(r.equals("")) msg("SEM CARTAO");

}

int checkAuth(String tag){
  int i;
  for (i = 0; i < VECTOR_SIZE_MAX; i++) {
    if (secure_card[i][0].equals(tag)) {
      if(i == 0){ //Retorna 2 se for master
        return 2;
      }
      return 1;  // Retorna 1 para usuarios cadastrados
    }
  }
  return 0;     // Retorna 0 caso não encontre ninguem
}

void checkCardToOpen(String tag){
  int r = checkAuth(tag);
  if(r == 1 || r == 2){
    msg("PORTAO ABERTO: " + tag);
    openDoor();
    return;
  }
  buzzer(2);
}

void api_command(String json){
  #ifdef DEBUG
    Serial.println(json);
  #endif
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  String pin_test = doc["password"].as<String>();
  pin_test.trim();
  if(!PIN.equals(pin_test)){
    #ifdef DEBUG
      Serial.println(doc["password"].as<String>());
    #endif
    msg("[API]: Senha invalida");
    return;
  }
  
  #ifdef DEBUG
    Serial.println("[API]: Senha aceita");
    Serial.println("[API]: Comando recebido: ");
    const char* r = doc["command"];
    Serial.println(r);
  #endif
  if(doc["command"].as<String>().equals("add_new_user")){
    addNewUser(false, doc["user"]);
  }
}

void checkCommand(String mensagem) {
  if (!mensagem.equals("")) {
    mensagem.trim();
    StringSplitter *split;
    if(mensagem.substring(0, mensagem.indexOf(" ")).equals("login")){                                     // Usuário faz login no sistema para conseguir modificar configurações
      mensagem.toLowerCase();
      String s1 = mensagem.substring(mensagem.indexOf(" "));
      s1.trim();
      login(s1);
      return;
    } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("open_door")) {                        // Abre o portão caso o pin esteja correto.
        String s1 = mensagem.substring(mensagem.indexOf(" "));
        s1.trim(); s1.toUpperCase();
        if(s1.equals("")){
          msg("Senha de root para abrir. [open_door <pin>]");
          return;
        }
        if(s1.equals(PIN)){
          openDoor();
        } else {
          msg("PIN incorreto.");
        }
        return;
    } else if(mensagem.substring(0, mensagem.indexOf(" ")).equals("api")){
      String json = mensagem.substring(mensagem.indexOf(" "));
      json.trim();
      if(!json.equals("")){
        api_command(json);
      }
      return;
    }
    if(_root){                                                                                // Verifica se o usuario do sistema entrou, caso aconteca, pode alterar as configurações do sistema. 
      if (mensagem.equals("add_new_user master")) {                                                // Adiciona um novo usuário/cartão no sistema.
        addNewUser(true, "master");
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("add_new_user")) {
        String s1 = mensagem.substring(mensagem.indexOf(" "));
        s1.trim(); s1.toUpperCase();
        addNewUser(false, s1);
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("change_wifi")) {
        mensagem.replace("change_wifi", "");
        split = new StringSplitter(mensagem, ',', 2);
        String ssid = split->getItemAtIndex(0);
        ssid.trim();
        String pass = split->getItemAtIndex(1);
        pass.trim();
        changeWifi(ssid, pass);
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("list_user")) {
        listUser();
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("remove_user")) {
        split = new StringSplitter(mensagem, ' ', 2);
        String tag = split->getItemAtIndex(1);
        tag.trim();
        removeUser(tag);
      }/* else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("only_master")) {
        String s1 = mensagem.substring(mensagem.indexOf(" "));
        s1.trim();
        s1.equals("enable") ? __only_master = true : __only_master = false;
        if (__only_master) msg("Sistema com acesso permitido apenas para master");
        else ("Sistema liberado para todos");
      }*/ else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("restart")) {
        restartEsp();
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("change_pin")) {
        split = new StringSplitter(mensagem, ' ', 2);
        String pin = split->getItemAtIndex(1);
        pin.trim();
        changePin(pin);
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("logout")) {
        logout();
      } else if (mensagem.substring(0, mensagem.indexOf(" ")).equals("clear_eeprom")) {
        clearEEPROM();
      } else {
        msg("Erro command [cmd: " + mensagem + "]");
      }
    } else {
      msg("Entre como root para mudar/ver configuração.");
    }
  }
}
