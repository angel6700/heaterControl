#include <SPI.h>
#include "RF24.h"


///// Coment the next line to disable Serial prints
//#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINT(x)   Serial.print(x)
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)
#endif


RF24 radio(7,8);

const uint8_t pipes[][6] = {"1Temp", "2Temp"};  

byte pin_LED=6;
byte pin_relay=5;
byte pinManualON=4;
byte pinManualOFF=3;
byte estadoAnterior=9;

byte pipeNo;
byte mensageRecibido;                                       
byte respuesta_ON=111;
byte respuesta_OFF=100;
byte respuesta = 123;

byte calefaccion_Encendida=0;
unsigned long maximoEncendida=7200000;
unsigned long miTimer=0;

boolean manualON;
boolean manualOFF;

char encendido_txt[]="ENCENDIDO";
char apagado_txt[]="APAGADO";

const char *estados[] ={apagado_txt, encendido_txt};

byte myVar=0;

void setup() 
{
  #ifdef DEBUG
    Serial.begin(115200);
  #endif
  
  pinMode(pin_LED, OUTPUT);
  digitalWrite(pin_LED, LOW);
  pinMode(pin_relay, OUTPUT);
  digitalWrite(pin_relay, LOW);
  pinMode(pinManualON, INPUT);
  digitalWrite(pinManualON, HIGH);
  pinMode(pinManualOFF, INPUT);
  digitalWrite(pinManualOFF, HIGH);
  
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
//  radio.setDataRate(RF24_1MBPS);
//  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(5,15);                 // Smallest time between retries, max no. of retries
  //radio.setCRCLength( RF24_CRC_8 );
  radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
  radio.openWritingPipe(pipes[1]);        // Both radios listen on the same pipes by default, and switch when writing
  radio.openReadingPipe(1,pipes[0]);
  radio.startListening();                 // Start listening
  
}

// en caso de que la calefaccion vaya a cambiar de estado, lo hace. En caso de que siga igual, no hace nada.
void calefaccion(byte nuevoEstado)
{
  if (nuevoEstado!=estadoAnterior)
  {
    DEBUG_PRINTLN(estados[nuevoEstado]);
    digitalWrite(pin_LED, nuevoEstado);
    digitalWrite(pin_relay, nuevoEstado);
    estadoAnterior=nuevoEstado;
    miTimer=millis();
  }
  else{
    DEBUG_PRINTLN("No hago nada");
  }
}

void loop()
{
  if (miTimer>=millis()){
    miTimer=0;
  }
  
  manualON=!digitalRead(pinManualON);
  manualOFF=!digitalRead(pinManualOFF);

  if (manualON)
  {
    calefaccion(1);
    while( radio.available(&pipeNo))
    {
      radio.read( &mensageRecibido, 1 );
      DEBUG_PRINT("Recibido: ");
      DEBUG_PRINT(mensageRecibido);
      DEBUG_PRINT("  de: ");
      DEBUG_PRINTLN(pipeNo);
      DEBUG_PRINTLN("Manual ON");
      radio.writeAckPayload(pipeNo,&respuesta_ON, 1 );  
      delay(100);
    }
  }
  
  else if (manualOFF)
  {
    calefaccion(0);
    while( radio.available(&pipeNo))
    {
      radio.read( &mensageRecibido, 1 );
      DEBUG_PRINT("Recibido: ");
      DEBUG_PRINT(mensageRecibido);
      DEBUG_PRINT("  de: ");
      DEBUG_PRINTLN(pipeNo);
      DEBUG_PRINTLN("Manual OFF");
      radio.writeAckPayload(pipeNo,&respuesta_OFF, 1 );  
      delay(100);
    }
  }

  else if (!manualON && !manualOFF)  // Modo automatico  
  { 
   
//    DEBUG_PRINTLN("Modo remoto");
    while( radio.available(&pipeNo))
    {
      radio.read( &mensageRecibido, 1 );
      DEBUG_PRINT("Recibido: ");
      DEBUG_PRINT(mensageRecibido);
      DEBUG_PRINT("  de: ");
      DEBUG_PRINTLN(pipeNo);
      calefaccion(mensageRecibido);
      radio.writeAckPayload(pipeNo,&respuesta, 1 );  
    }
      
 
  }
}

