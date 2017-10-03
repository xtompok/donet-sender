#include <SPI.h>
#include "RF24.h"

RF24 radio(14, 10);

struct __attribute__((packed)) packet {
  uint8_t s_addr;
  uint8_t d_addr;
  uint8_t counter;
  uint8_t command;
  uint8_t data[13];
};

byte address[] = "DoNet";

#define CMD_PING    0x01
#define CMD_PWM_ONE 0x02
#define CMD_PWM_THREE 0x03
#define CMD_UART_SEND 0x04
#define CMD_UART_RECV 0x05

#define CMD_PING_ACK      0xE1
#define CMD_PWM_ONE_ACK       0xE2
#define CMD_PWM_THREE_ACK 0xE3
#define CMD_UART_SEND_ACK 0xE4
#define CMD_UART_RECV_ACK 0xE5

#define ERR_OK 0x00
#define ERR_TIMEOUT 0x01
#define ERR_OTHER 0xEE


struct packet packetIn;
struct packet packetOut;

#define ADDRESS 0x01

#define MAX_TRIES 10 
#define TIMEOUT 200000 //In us

#define MAX_DEVICE_ID 10

#ifdef DEBUG_SERIAL
#define DPRINTLN(...) Serial.println(__VA_ARGS__)
#define DPRINT(...) Serial.print(__VA_ARGS__)
#else
#define DPRINTLN(...) do {} while(0)
#define DPRINT(...) do {} while(0)
#endif

void setup() {
  Serial.begin(57600);
  radio.begin();

  radio.setPALevel(RF24_PA_MAX);
  radio.openWritingPipe(address);
  radio.openReadingPipe(1, address);

  // Start the radio listening for data
  radio.startListening();
}

uint8_t wait_ack(uint8_t id,uint8_t cmd){
  if (packetIn.d_addr != ADDRESS) return 0;
  if (packetIn.s_addr != id) return 0;
  if (packetIn.command != cmd+0xE0) return 0;
  return 1;
}

uint8_t wait_for_reply(uint8_t id, uint8_t cmd){
  unsigned long start_wait = micros();
  uint8_t error = false;

  do {
    while (!radio.available()){
      if (micros() - start_wait > TIMEOUT){
        error = ERR_TIMEOUT;
        break;
      }
    }
    radio.read(&packetIn,sizeof(struct packet));
    if (wait_ack(id,cmd)){
      break;
    }
  } while (!error);
  return error;
}

void scan() {
  packetOut.command = CMD_PING;
  packetOut.s_addr = ADDRESS;
  packetOut.data[0] = 0xDE;
 
  for (int i=0;i<MAX_DEVICE_ID;i++){
   
    packetOut.d_addr = i;
    uint8_t tries;
    tries = send_packet();
    
    if (tries == MAX_TRIES){
      Serial.print("Scan;SF;");  
      Serial.println(i);
      continue;
    }

    uint8_t error;
    error = wait_for_reply(i,CMD_PING);
    
    if (error==ERR_TIMEOUT){
      Serial.print("Scan;RT;");
      Serial.println(i);
      continue;
    } else if (error){
      Serial.print("Scan;RE;");
      Serial.println(i);
      continue;
    }
   
    Serial.print("Scan;OK;");
    Serial.print(i);
    Serial.print(";");
    Serial.println(packetIn.data[0],HEX);
  }  
}

uint8_t send_packet(){
    uint8_t tries;
    tries = 0;

    radio.stopListening();
    
    while (tries < MAX_TRIES){
      if (radio.write( &packetOut, sizeof(struct packet) )){
        break;
      }
      tries++;  
    }
    radio.startListening(); 
    return tries;
}

uint8_t getChar(){
  while(!Serial.available()){}
  uint8_t ch;
  ch = Serial.read();
#ifdef DEBUG
  Serial.print("'");
  Serial.print(ch,HEX);
  Serial.print("'");
#endif
  return ch;  
}

void prepareUartData(){
  uint8_t len;
  len = getChar();
  packetOut.data[0] = len;
  for (int i=0;i<len;i++){
    packetOut.data[i+1] = getChar();
  }  
}

void readUartData(){
  // TODO implement  
}

void loop() {
  unsigned long start_wait;
  start_wait = millis();
  while(!Serial.available()){
      /*if (millis()-start_wait > 1000){
        scan();
        return;  
      }*/
  }
    DPRINTLN("Waiting for command completion");
    uint8_t ok = 1;
    packetOut.s_addr = ADDRESS;
    packetOut.d_addr = getChar();
    packetOut.command = getChar();
    switch(packetOut.command){
      case CMD_PWM_ONE:
        packetOut.data[0] = getChar();
        Serial.print("CMD_ONE;");
        break;
      case CMD_PWM_THREE:
        Serial.print("CMD_THREE;");
        packetOut.data[0] = getChar();
        packetOut.data[1] = getChar();
        packetOut.data[2] = getChar();
        break;
      case CMD_UART_SEND:
        prepareUartData();
        Serial.print("CMD_UA_S;");
        break;
      case CMD_UART_RECV:
        packetOut.data[0] = getChar();
        Serial.print("CMD_UA_R;");
        readUartData();
        break;
      default:
        Serial.print("CMD_UNK;");
        break;
    }
    Serial.print(packetOut.d_addr);
    
    if (ok){
      DPRINTLN("Sending packet");
      uint8_t tries;
      tries = send_packet();
      if (tries == MAX_TRIES){
        
        Serial.println(";SERR;");
        return;
      }
      if(uint8_t error = wait_for_reply(packetOut.d_addr,packetOut.command)){
        Serial.println(";NACK;");
      } else {
        Serial.println(";OK;");  
      }
    }  
  
} // Loop

