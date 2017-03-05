#include <SPI.h>
#include "RF24.h"
#include "LowPower.h"
#include <DS3231.h>
#include <PCD8544.h>

///// Coment the next line to disable Serial prints
#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x)   Serial.println(x)
#define DEBUG_PRINT(x)   Serial.print(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

//////// Version will appear on LCD on power up and after reset
#define VERSION 2.0

///////// Definition of lcd: nokia 5100 display
static PCD8544 lcd(5, 6, 7, 4, 8);

/////////  RTC DS3231 using: http://www.rinkydinkelectronics.com/library.php?id=73
//DS3231  reloj_DS3231(SDA, SCL);


//////// Radio module nrf24l01+ using library: tmrh20 RF24
RF24 radio(9, 10);

//// Pipes for radio comunication
const uint8_t pipes[][6] = {"1Temp", "2Temp"};

//////////////// Global variables definition //////////////////
boolean goingStandby = false;
boolean standby = false;

boolean currentMode = false;
boolean error = true;
boolean batteryLow=false;
boolean manualModeON = false;
boolean manualModeOFF = false;
boolean autoMode = true;

boolean heatingOFF = 0;
boolean heatingON = 1;
boolean booting = true;

byte pin_thermistor = A0;
byte pin_thermistor_power = A2;
byte pin_button_tempUP = 3;
byte pin_button_tempDOWN = 2;
byte pin_backLight = A3;
byte pin_batteryLevel = A1;

int errorCount = 0;
int repaintCount = 0;

// *************************************************************************************************
int sleepTimeMitutes=1;
int batteryCheckCounter=0;
int batteryChecksPerDay=1;
int sleepTimeRepeats=(sleepTimeMitutes*60/8);
int batteryChecksLimit=(batteryChecksPerDay*24*60)/sleepTimeMitutes;
// *************************************************************************************************

byte message=0;

volatile float setPoint = 21.5;
//float init_setPoint = 23.0;
float old_setPoint=0;

float batteryVoltage = 1.5;  //Battery measured from one battery (AA or AAA) should be above batteryLowVoltaje
float batteryLowVoltaje = 1.02;
float roomTemperature;

float thresholdHigh = 0.1;
float thresholdLow = 0.3;
float temperatureStep = 0.5;

float tempMax = 35.0;
float tempMin = 5.0;

volatile boolean standbyUP = false;
volatile boolean standbyDOWN = false;
volatile boolean firstButtonPress = true;
volatile boolean backlightON = false;
volatile unsigned long debounceTimerUP = 0;
volatile unsigned long debounceTimerDOWN = 0;


unsigned long debounce = 500;
unsigned long backlightTimer = 4000;  // ms to keep on the backlight
int tempCounter=0;


// Calculate thermistor temperature as T=a*x+b
float a = -0.0897925486;
float b = 70.8304569;

//static const byte DEGREES_CHAR = 1;
//static const byte degrees_glyph[] = { 0x00, 0x07, 0x05, 0x07, 0x00 };
//// A bitmap graphic (10x2) of a thermometer...
//static const byte THERMO_WIDTH = 10;
//static const byte THERMO_HEIGHT = 2;
//static const byte thermometer[] = { 0x00, 0x00, 0x48, 0xfe, 0x01, 0xfe, 0x00, 0x02, 0x05, 0x02,
//                                    0x00, 0x00, 0x62, 0xff, 0xfe, 0xff, 0x60, 0x00, 0x00, 0x00
//                                  };

float measureTemperature(int pin) 
{
  digitalWrite(pin_thermistor_power, HIGH);
  int raw = 0;
  for (int i = 0; i < 5; i++) 
  {
    raw += analogRead(pin);
    delay(5);
  }
  digitalWrite(pin_thermistor_power, LOW);
  raw = raw / 5;
  float calculatedTemperature = a * raw + b;
  return calculatedTemperature;
}  ////////////////// end measureTemperature(int pin)  //////////////////////////////


float measureBatteryVoltage(int pin) 
{
  int raw = 0;
  for (int i = 0; i < 5; i++) 
  {
    raw += analogRead(pin);
    delay(5);
  }
  raw = raw / 5;
  float batteryVoltage = raw * 3.3 / 1024;
  return batteryVoltage;
}  /////////////////////////// end measureBatteryVoltage(int pin)  //////////////////////////////

void wakeUpButtonUp()
//// This function will be called when the pushButton tempUP 
{
  if ((millis() - debounceTimerUP) > debounce)
  {
    debounceTimerUP = millis();
    if (setPoint < tempMax && !firstButtonPress)
    {
      setPoint += temperatureStep;
    }
    digitalWrite(pin_backLight, LOW);  // Turn on backligh
    backlightON = true;
    standbyUP = true;
    firstButtonPress = false;
  }
} //////////////////////////////////// end wakeUpButtonUp()  ////////////////////////////


void wakeUpButtonDown()
//// This function will be called when the pushButton tempDOWN
{
  if ((millis() - debounceTimerDOWN) > debounce)
  {
    debounceTimerDOWN = millis();
    if (setPoint > tempMin && !firstButtonPress)
    {
      setPoint -= temperatureStep;
    }
    digitalWrite(pin_backLight, LOW);  // Turn off backlight
    backlightON = true;
    standbyDOWN = true;
    firstButtonPress = false;
  }
} ////////////////////////////// end wakeUpButtonDown() //////////////////////////////

void displayWellcome()
{
  lcd.setCursor(0, 0);
  lcd.print("Version: ");
  lcd.print(VERSION);
  lcd.setCursor(0, 2);
  lcd.print("Creado el:");
  lcd.setCursor(0, 3);
  lcd.print(__DATE__);
  lcd.setCursor(0, 4);
}   ////////////////////////////// end displayWellcome()  //////////////////////////////

void displayError()
{
  lcd.setCursor(0, 0);
  lcd.print("Error de");
  lcd.print("comunicacion");
} ////////////////////////////// end displayError()  //////////////////////////////

void setup()
{
  lcd.begin(84, 48);   // clears the screen and buffer
  //lcd.createChar(DEGREES_CHAR, degrees_glyph);

  //reloj_DS3231.begin();

  //reloj_DS3231.setTime(18, 44, 0);  // we can use this line to set the clock one and the comment it again

  pinMode(pin_button_tempDOWN, INPUT);
  pinMode(pin_button_tempUP, INPUT);
  pinMode(pin_thermistor_power, OUTPUT);
  digitalWrite(pin_thermistor_power, LOW);
  pinMode(pin_backLight, OUTPUT);
  digitalWrite(pin_backLight, HIGH);
  backlightON = false;

#ifdef DEBUG
  Serial.begin(115200);
#endif

  radio.begin();
  radio.setDataRate(RF24_250KBPS);        // Found 250KBPS to perform better and most reliable 
  //  radio.setDataRate(RF24_1MBPS);
  //  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(5, 15);                // Smallest time between retries, max no. of retries
  radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
  radio.openWritingPipe(pipes[0]);        // Both radios listen on the same pipes by default, and switch when writing
  radio.openReadingPipe(1, pipes[1]);
  radio.startListening();                 // Start listening

  error = true;
  byte i = 0;
  while (error && (i < 20) ) 
  {
    error = heating(heatingOFF);
    i += 1;
    lcd.setCursor(0, 5);
    lcd.clearLine();
    lcd.print(i);
  }
  if (!error)
  {
    currentMode = heatingOFF;
    displayWellcome();
  }
  else
  {
    displayError();
  }

  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Temp:");
  lcd.setCursor(0, 2);
  lcd.print("Cale:");
  message=0;

  roomTemperature = measureTemperature(pin_thermistor);
  batteryVoltage = measureBatteryVoltage(pin_batteryLevel);

  attachInterrupt(0, wakeUpButtonUp, RISING);
  attachInterrupt(1, wakeUpButtonDown, RISING);
} /////////////////////////////////////////// setup()  ////////////////////////////////////////////////////

boolean heating(boolean newMode)
{
  boolean errorC = true;
  byte ack_autoMode = 123;
  byte ack_ManualON = 111;
  byte ack_ManualOFF = 100;
  radio.powerUp();
  radio.stopListening();
  byte payload;
  //noInterrupts();
  if (newMode == heatingON)
  {
    payload = 1;
    DEBUG_PRINTLN("Encendiendo calefaccion en [heating]");
  }
  else if (newMode == heatingOFF)
  {
    payload = 0;
    DEBUG_PRINTLN("Apagando calefaccion en [heating]");
  }

  // Send a message to radio_server
  byte gotByte;
  if (!radio.write( &payload, 1 ))
  {
    DEBUG_PRINTLN(F("failed."));
  }
  else
  {
    while (radio.isAckPayloadAvailable())
    {
      //      DEBUG_PRINTLN(F("Blank Payload Received."));

      radio.read( &gotByte, 1 );
      DEBUG_PRINT("Automensaje recibido: ");
      DEBUG_PRINTLN(gotByte);
      if (gotByte == ack_autoMode)
      {
        DEBUG_PRINTLN("Modo AUTOMATICO");
        autoMode = true;
        manualModeON = false;
        manualModeOFF = false;
        errorC = false;
      }
      else if (gotByte == ack_ManualON)
      {
        DEBUG_PRINTLN("Modo Manual ON");
        manualModeON = true;
        manualModeOFF = false;
        autoMode = false;
        errorC = false;

      }
      else if (gotByte == ack_ManualOFF)
      {
        DEBUG_PRINTLN("Modo Manual OFF");
        manualModeOFF = true;
        manualModeON = false;
        autoMode = false;
        errorC = false;
      }
    }
  }

  DEBUG_PRINTLN(errorC);
  
  radio.powerDown();

  if (!errorC)
  {
    DEBUG_PRINTLN("NOT ERROR");
    if (manualModeON)
    {
      currentMode = heatingON;
      if (message!=1)
      {
        lcd.setCursor(0, 4);
        lcd.clearLine();
        lcd.print("Encend. manual");
        message=1;
      }
    }
    else if (manualModeOFF)
    {
      currentMode = heatingOFF;
      if (message!=2)
      {
        lcd.setCursor(0, 4);
        lcd.clearLine();
        lcd.print("Apagado manual");
        message=2;
      }
    }
    else if (autoMode)
    {
      if (newMode == heatingON)
      {
        if (message!=3)
        {
          lcd.setCursor(0, 4);
          lcd.clearLine();
          lcd.print("Calentando!!");
          message=3;
        }
      }
      else if (newMode == heatingOFF)
      {
        if (message!=4)
        {
          lcd.setCursor(0, 4);
          lcd.clearLine();
          message=4;
        }
      }
    }
  }
  else
  {
    if (message!=99)
    {
      DEBUG_PRINTLN("ERROR");
      lcd.setCursor(0, 4);
      lcd.clearLine();
      lcd.print("*** Error ***");
      message=99;
    }
  }

  return errorC;
}  //////////////////////////////// end heating(boolean newMode) ///////////////////////////////////////


void loop() 
{
  DEBUG_PRINT("sleepTimeRepeats:");
  DEBUG_PRINTLN(sleepTimeRepeats);
  DEBUG_PRINT("batteryChecksLimit:");
  DEBUG_PRINTLN(batteryChecksLimit);

  if (debounceTimerUP >= millis())  // this is probably not needed but I just pretend to take care of the millis overflow
  {
    debounceTimerUP = 0;
    debounceTimerDOWN = 0;
  }
  
  if (((millis() - debounceTimerUP) > backlightTimer) && ((millis() - debounceTimerDOWN) > backlightTimer)) 
  {   //////////// Switch off the backlight after (backlightTimer) and go to sleep //////////////////////////////
    digitalWrite(pin_backLight, HIGH); //apaga la luz
    backlightON = false;
    debounceTimerUP = 0;
    debounceTimerDOWN = 0;
    firstButtonPress = true;
    repaintCount = 0;

    /////////////////  temporal block to show the time spent awake, between sleps  ///////////
    int runTime=millis()-tempCounter;
    lcd.setCursor(20,5);
    //lcd.clearLine();
    lcd.print(runTime,1);
    delay(1000);
    //////////////////////////////////////////////////////////////////////////////////////////
    DEBUG_PRINTLN("Ahora me duermo");
    delay(20);
    for (byte ii = 0; ii < sleepTimeRepeats; ii++)
    {
      //radio.powerDown();
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }   //////////////////////////////////////// end of sleep block  /////////////////////////////////////


  ///////////////////////  temporal variable to show the time spent awake, between sleps  //////////////
  tempCounter=millis();
  //////////////////////////////////////////////////////////////////////////////////////////////////////
  
  batteryCheckCounter+=1;
  
  
  if (standbyUP || standbyDOWN)   ///////////// One button has been pressed  ////////////////////////////
  {
    detachInterrupt(0);
    detachInterrupt(1);
    standbyUP = false;
    standbyDOWN = false;
    repaintCount = 0;
    DEBUG_PRINTLN("Algun boton pulsado");
    //    delay(20);
    lcd.setPower(true);
    if (digitalRead(pin_button_tempDOWN) && digitalRead(pin_button_tempUP))  //Los dos botones pulsados
    {
      //noInterrupts();
      DEBUG_PRINTLN("Dos botones pulsados. Entrar en standby");
      goingStandby = true;
      standby = false;
      delay(500); // Tiempo para que de tiempo a soltar los botones
    }
    else
    {
      attachInterrupt(0, wakeUpButtonUp, RISING);
      attachInterrupt(1, wakeUpButtonDown, RISING);
      //interrupts();
      DEBUG_PRINTLN("Ahora me despierto"); //Solo pulsado un boton
      if (standby) 
      {
        //lcd.setCursor(0, 4);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Temp:");
        lcd.setCursor(0, 2);
        lcd.print("Cale:");
      }
      goingStandby = false;
      standby = false;
    }
  }   ///////////////////////////// end of button press analisys ////////////////////////////////////////

  
  
  //char* timestr = reloj_DS3231.getTimeStr(FORMAT_SHORT);

  char timestr[] = "00:01";      ////////////////////// used in case not RTC is connected ///////////////

  //  if (timestr[0]== '1' || timestr[0]=='2' || timestr[1]=='9')
  //{
  //    lcd.setPower(true);
  //  }

  lcd.setCursor(25, 0);
  lcd.print(timestr);

  lcd.setCursor(0, 0);
  lcd.print(errorCount);

  
  ////DEBUG_PRINTLN(setPoint);
  //lcd.setCursor(0, 4);
  //lcd.drawBitmap(thermometer, THERMO_WIDTH, THERMO_HEIGHT);

  if (!goingStandby)
  {                         ////////////////////// normal operation when awake ///////////////////////////
    if (repaintCount < 1)
    {
      roomTemperature = measureTemperature(pin_thermistor);
      //roomTemperature*=(-1);

      
      lcd.setCursor(30, 1);
      lcd.print(roomTemperature, 1);

      
      //lcd.clearLine();

      if (autoMode)
      {
        if (setPoint!=old_setPoint)
        {
          lcd.setCursor(30,2);
          lcd.print(setPoint, 1);
          old_setPoint=setPoint;
        }
      }

      if (batteryCheckCounter>batteryChecksLimit)
      {
        batteryVoltage = measureBatteryVoltage(pin_batteryLevel);
        batteryCheckCounter=0;
        if (batteryVoltage < batteryLowVoltaje)
        {
          if (!batteryLow)
          {
            batteryLow=true;
            lcd.setCursor(0, 3);
            lcd.clearLine();
            lcd.print("Bateria baja!");
          }
        }
        else
        {
          if (batteryLow)
          {
            batteryLow=false;
            lcd.setCursor(0, 3);
            lcd.clearLine();
          }
        }
      }
      
      // if temperature is above setpoint, send message to turn off heating
      if (roomTemperature >= (setPoint + thresholdHigh))
      {
        if (currentMode == heatingON || currentMode == heatingOFF)
        {
          DEBUG_PRINTLN("roomTemperature alta! Enviando mensaje OFF...");
          //long t_ini = millis();
          error = true;
          byte i = 0;
          while (error && (i < 20) )
          {
            //DEBUG_PRINTLN("Apagando calefaccion...");
            error = heating(heatingOFF);
            i += 1;
            lcd.setCursor(0, 5);
            lcd.clearLine();
            lcd.print(i);
          }
          if (!error)
          {
            currentMode = heatingOFF;
          }
          else
          {
            errorCount += 1;
          }
          //DEBUG_PRINT("Tiempo en calefaccion=");
          //DEBUG_PRINTLN(millis() - t_ini);
        }
      }
      else if (roomTemperature < (setPoint - thresholdLow))
      {
        if (currentMode == heatingOFF || currentMode == heatingON)  
        {
          DEBUG_PRINTLN("roomTemperature baja! Enviando mensaje ON...");
          //long t_ini = millis();
          error = true;
          byte i = 0;
          while (error && (i < 20)) 
          {
            //DEBUG_PRINTLN("Encendiendo calefaccion....");
            error = heating(heatingON);
            i += 1;
            lcd.setCursor(0, 5);
            lcd.clearLine();
            lcd.setCursor(50, 5);
            lcd.print(i);
          }
          if (!error)
          {
            currentMode = heatingON;
          }

          else
          {
            errorCount += 1;
          }

          //DEBUG_PRINT("Tiempo en calefaccion=");
          //DEBUG_PRINTLN(millis() - t_ini);
        }
      }    
    

   
    }
    repaintCount += 1; 
    
  }  //////////////////////////////////// end of normal operation when awake //////////////////////////////////////


  else ////////////////////////////////////  if (goingStandby)    ///////////////////////////////////////////////////////////
  {  //////////////  in case of both buttons where pressed: switch-off heating (if needed), and powerDown the display
    if (currentMode == heatingON)
    {
      DEBUG_PRINTLN("Apagando... Enviando mensaje OFF...");
      // long t_ini = millis();

      error = true;
      byte i = 0;
      while (error && (i < 20) ) 
      {
        DEBUG_PRINTLN("Apagando calefaccion...");
        error = heating(heatingOFF);
        i += 1;
        lcd.setCursor(0, 5);
        lcd.clearLine();
        lcd.print(i);
      }
      if (!error)
      {
        DEBUG_PRINTLN("Antes de dormir he apagado la calefaccion. Ahora Standby..");
        currentMode = heatingOFF;
        backlightON = false;
        lcd.clear();
        lcd.setCursor(5, 4);
        lcd.print("Buenas noches");
        delay(3000);
        lcd.setPower(false);
        EIFR = bit (INTF0);  // clear flag for interrupt 0
        EIFR = bit (INTF1);  // clear flag for interrupt 1

        firstButtonPress = true;
        attachInterrupt(0, wakeUpButtonUp, RISING);
        attachInterrupt(1, wakeUpButtonDown, RISING);
        digitalWrite(pin_backLight, HIGH); //apaga la luz      
        //radio.powerDown();
        standby = true;
      }
      //DEBUG_PRINT("Tiempo en calefaccion=");
      //DEBUG_PRINTLN(millis() - t_ini);
    }
    else
    { // if (currentMode == heatingOFF)
      if (!standby) 
      {
        DEBUG_PRINTLN("La calefaccion ya estaba apagada. Así que solo queda entrar en standby");
        // setPoint=init_setPoint;
        backlightON = false;
        lcd.clear();
        lcd.setCursor(5, 4);
        lcd.print("Buenas noches");
        delay(3000);
        lcd.setPower(false);
        EIFR = bit (INTF0);  // clear flag for interrupt 0
        EIFR = bit (INTF1);  // clear flag for interrupt 1

        firstButtonPress = true;
        attachInterrupt(0, wakeUpButtonUp, RISING);
        attachInterrupt(1, wakeUpButtonDown, RISING);
        digitalWrite(pin_backLight, HIGH); //apaga la luz
        radio.powerDown();
        standby = true;
      }
    }
  }   ///////////////////////////  end of going into deep standby  ////////////////////////////////////////////////

  
} ///////////////////////////////////////   end loop()   ///////////////////////////////////////