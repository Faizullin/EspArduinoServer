#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <FS.h>
#include <Wire.h>
#include <Servo.h>

#define KNOWN A
#define LEDpin D4
#define WIRE_SENSOR 8
#define RASP_BUTTON_INPUT D7
#define SERVO1_OUTPUT D4
#define SERVO2_OUTPUT D6
#define myssid "Заходи сюда!"
#define mypassword "12344321"
#define PRINT 1
#define DIOD_COMMAND 2
#define LOCALIP_COMMAND 3

ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);

Servo myservo1,myservo2;


bool button_wait(unsigned long l){
  digitalWrite(LEDpin,LOW);
  bool res = false;
  uint16_t timer = millis();
  while(millis() - timer <l){
    if(!res){
      if(!digitalRead(RASP_BUTTON_INPUT)){res=true;digitalWrite(LEDpin,HIGH);Serial.println("DETECTED");}
    };
    delay(50);
  }
  return res;
}
void mig(uint8_t l=1,uint16_t timer = 500){
  for(uint8_t i = 0;i<l;i++){
    digitalWrite(LEDpin,HIGH);
    delay(timer);
    digitalWrite(LEDpin,LOW);
    delay(timer);
  }
}
bool rasp_con = false;
void handle_connect();
void handle_OnConnect();
void handle_NotFound();
void handle_ledOn();
void handle_ledOff();
void handle_led();
void senddata();
//void receiveEvent();

void setup() {
  #ifdef PRINT
    Serial.begin(9600);
    Serial.println("START");
  #endif
  Wire.begin();
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount failed");
  } else {
    Serial.println("SPIFFS Mount succesfull");
  }
  pinMode(LEDpin, OUTPUT);digitalWrite(LEDpin,LOW);
  pinMode(RASP_BUTTON_INPUT,INPUT_PULLUP);
  myservo1.attach(SERVO1_OUTPUT);
  myservo2.attach(SERVO2_OUTPUT);
  mig();
  IPAddress myIP;
  if(button_wait(3000)){
    mig(3);rasp_con = true;
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    File file = SPIFFS.open("/conf.txt","r");
    if(!file){
      #ifdef PRINT
        Serial.println("There was an error opening the file for writing");
       #endif
       
       return;
    }
    std::vector<String> v;
    while (file.available()) {
      v.push_back(file.readStringUntil('\n'));
    }
    int index=0;
    String ssid,password;
   
    for (String s : v) {
      index = s.indexOf(';');
      ssid = s.substring(0,index);
      password = s.substring(index+1,s.length());
      #ifdef PRINT
      Serial.println(ssid+(String)ssid.length()+','+password+(String)password.length());
       #endif
      wifiMulti.addAP(ssid.c_str(), password.c_str());
    }
    while(wifiMulti.run() != WL_CONNECTED) {
      delay ( 500 ); 
      #ifdef PRINT
      Serial.print ( "." );
      #endif
   
    }
    ssid = WiFi.SSID();
    myIP = WiFi.localIP();
    digitalWrite(LEDpin,HIGH);
    #ifdef PRINT
      Serial.println("WiFi connected to "+ssid);
      Serial.print("IP address: ");
      Serial.println(myIP);
    #endif
    
  }else{
    IPAddress LOCAL_IP(192,168,4,1);
    IPAddress GATEWAY(192,168,4,1);
    IPAddress SUBNET(255,255,255,0);
    WiFi.softAPConfig(LOCAL_IP, GATEWAY, SUBNET);
    WiFi.softAP(myssid, mypassword);
    myIP = WiFi.softAPIP();
    #ifdef PRINT
      Serial.print("AP IP address: ");
      Serial.println(myIP);
    #endif
  }
  
  Wire.beginTransmission(WIRE_SENSOR);
  Wire.write(LOCALIP_COMMAND);
  for(uint8_t i=0;i<4;i++){
    Wire.write(myIP[i]);
  }
  Wire.endTransmission();
  mig(2);
  delay(100);
  server.on("/", handle_OnConnect);
  server.on("/ledon", handle_ledOn);
  server.on("/ledoff", handle_ledOff);
  server.on("/readData",readdata);
  server.on("/connect", handle_connect);
  server.on("/addWiFi",addWiFi);
  server.on("/deleteWiFi", deleteWiFi);
  server.on("/get", handle_file);
  server.on("/servo", handle_servo);
  server.begin();
  #ifdef PRINT
      Serial.println( "HTTP server started" );
  #endif
  
}

void loop() { 
  server.handleClient();
}
void handle_connect(){
  File file = SPIFFS.open("/wifi.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Problem with filesystem!\n");
    return ;
  }
  server.streamFile(file, "text/html");
  file.close();
}
void addWiFi(){
  if( server.arg("s")=="" || server.arg("p")==""){
    server.sendHeader("Location","/", true);
    server.send ( 302, "text/plain", "");   
    return;
  }
  File file = SPIFFS.open("/conf.txt","a+");
  if(!file){
     file.close();
     return;
  }
  file.print((String)server.arg("s")+";"+(String)server.arg("p")+"\n");
  server.sendHeader("Location","/", true);
  server.send ( 302, "text/plain", ""); 
  file.close();
}
void deleteWiFi(){
  if(SPIFFS.exists("/conf.txt")){
    SPIFFS.remove("/conf.txt");
  }
  File file = SPIFFS.open("/conf.txt","w");
  if(!file){
     Serial.println("There was an error opening the file for writing");
     return;
  }
  server.send (200, "text/plain", "");
  file.close();
}
void handle_file(){
  if (server.arg("path")== "") {
    return;
  }
  String path = server.arg("path");
  if(SPIFFS.exists("/"+path)){
    File file = SPIFFS.open("/"+path, "r");
    if (!file) {
      server.send(500, "text/plain", "Problem with filesystem!\n");
      return ;
    }
    server.streamFile(file, "text/plain");
    file.close();
  }
  server.send (404, "text/plain", "Файл не найден");
}
void handle_ledOn() {
  Wire.beginTransmission(WIRE_SENSOR);
  Wire.write(DIOD_COMMAND);//diod
  Wire.write(1);
  Wire.endTransmission();   
  server.send (200, "text/plain", "");
}
void handle_ledOff(){
  Wire.beginTransmission(WIRE_SENSOR);
  
  Wire.write(DIOD_COMMAND);
  Wire.write(0);
  Wire.endTransmission();
  server.send (200, "text/plain", "");
}
void handle_OnConnect() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Problem with filesystem!\n");
    return ;
  }
  server.streamFile(file, "text/html");
  file.close();
}
void handle_servo() {
  if(server.arg("r1")=="" && server.arg("r2")==""){
    server.sendHeader("Location","/", true);
    server.send ( 302, "text/plain", "");   
    return;
  }
  int val1 = server.arg("r1").toInt(),
      val2 = server.arg("r2").toInt();
  bool error = false;
  #ifdef PRINT
    Serial.print(val1);
    Serial.println(val2);
  #endif
  if(val1>=0 && val1<180){
    myservo1.write(val1);
  }else{
    error=true;
  }
  if(val2>=0 && val2<180){
    myservo2.write(val2);
  }else{
    error=true;
  }
  if(error){
    server.send(500, "text/plain", "");
  }else{
    server.send(200, "text/plain", "");
  }
  
  
  
}
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

void readdata(){
  Wire.requestFrom(WIRE_SENSOR, 8);
  int data[4]{0,0,0,0};
  uint8_t j_inTime = 0;byte x_inTime;
  for (int i = 0; i < 4; i++){
    x_inTime = Wire.read();
    j_inTime++;
    if (j_inTime == 1){
      data[i] = (x_inTime << 8) | Wire.read();
      j_inTime = 0;
    }
  }
  server.send(200, "text/plain", "{\"temp\":\""+(String)data[0]+"\",\"hum\":\""+(String)data[1]+"\",\"gas\":\""+(String)data[2]+"\",\"count\":\""+(String)data[3]+"\"}");
}
