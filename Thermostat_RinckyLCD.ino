#include <SPI.h>
#include "RF24.h"     // https://github.com/TMRh20/RF24
#include "LowPower.h" // https://github.com/rocketscream/Low-Power
#include <DS3231.h>   // http://www.rinkydinkelectronics.com/
#include <LCD5110_Basic.h>  // http://www.rinkydinkelectronics.com/

///// Coment the next line to disable Serial prints
//#define DEBUG
#define DEBUGDISPLAY

#ifdef DEBUG
#define DEBUG_PRINTLN(x)   Serial.println(x)
#define DEBUG_PRINT(x)   Serial.print(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

#ifdef DEBUGDISPLAY
#define DEBUG_LCD(x) debugLCD(x)
#else
#define DEBUG_LCD(x)
#endif


//////// Version will appear on LCD on power up and after reset
#define VERSION 2.0

///////// Definition of lcd: nokia 5100 display
static LCD5110 lcd(5, 6, 7, 4, 8);
extern uint8_t SmallFont[];

DS3231  RTC(SDA, SCL);
Time t;
boolean DST = 0;


//////// Radio module nrf24l01+ using library: tmrh20 RF24
RF24 radio(9, 10);

//// Pipes for radio comunication
const uint8_t pipes[][6] = {"1Temp", "2Temp"};

//////////////// Global variables definition //////////////////
boolean goingStandby = false;
boolean standby = false;

boolean currentMode = false;
boolean error = true;
boolean batteryLow = false;
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

unsigned int errorCount = 0;
unsigned int old_errorCount = 0;
unsigned int repaintCount = 0;
unsigned int loopCount = 0;

// *************************************************************************************************
int sleepTimeMitutes = 5;
int batteryCheckCounter = 0;
int batteryChecksPerDay = 1;
int sleepTimeRepeats = (sleepTimeMitutes * 60 / 8);
int batteryChecksLimit = (batteryChecksPerDay * 24 * 60) / sleepTimeMitutes;
// *************************************************************************************************

byte message = 0;

volatile float setPoint = 21.5;
//float init_setPoint = 23.0;
float old_setPoint = 0;

float batteryVoltage = 1.5;  //Battery measured from one battery (AA or AAA) should be above batteryLowVoltaje
float batteryLowVoltaje = 1.02;
float roomTemperature;

float thresholdHigh = 0.1;
float thresholdLow = 0.3;
float temperatureStep = 0.5;

float tempMax = 35.0;
float tempMin = 15.0;

volatile boolean standbyUP = false;
volatile boolean standbyDOWN = false;
volatile boolean firstButtonPressUP = true;
volatile boolean firstButtonPressDOWN = true;
volatile boolean backlightON = false;
volatile unsigned long debounceTimerUP = 0;
volatile unsigned long debounceTimerDOWN = 0;


unsigned long debounce = 500;
unsigned long backlightTimer = 3000;  // ms to keep on the backlight
unsigned long tempCounter = 0;


// Calculate thermistor temperature as T=a*x+b
float a = -0.0897925486;
float b = 70.8304569;
//float a = -0.12344288;
//float b = 86.8307006;

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
  delay(5);
  int raw = 0;
  for (int i = 0; i < 7; i++)
  {
    raw += analogRead(pin);
    delay(5);
  }
  digitalWrite(pin_thermistor_power, LOW);
  raw = raw / 7;
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
    if (setPoint < tempMax && !firstButtonPressUP)
    {
      setPoint += temperatureStep;
    }
    digitalWrite(pin_backLight, LOW);  // Turn on backligh
    backlightON = true;
    standbyUP = true;
    firstButtonPressUP = false;
  }
} //////////////////////////////////// end wakeUpButtonUp()  ////////////////////////////


void wakeUpButtonDown()
//// This function will be called when the pushButton tempDOWN
{
  if ((millis() - debounceTimerDOWN) > debounce)
  {
    debounceTimerDOWN = millis();
    if (setPoint > tempMin && !firstButtonPressDOWN)
    {
      setPoint -= temperatureStep;
    }
    digitalWrite(pin_backLight, LOW);  // Turn on backlight
    backlightON = true;
    standbyDOWN = true;
    firstButtonPressDOWN = false;
  }
} ////////////////////////////// end wakeUpButtonDown() //////////////////////////////

boolean checkDST(Time t)
{
  if (t.mon >= 10 || t.mon <= 3) {
    if (t.mon == 10) {
      if (t.date > 24 and t.dow == 7) {
        if (t.hour >= 3) {
          DST = 0;
        }
        else {
          DST = 1;
        }
      }
      else {
        if (t.date + (7 - t.dow) > 31) {
          DST = 0;
        }
        else {
          DST = 1;
        }
      }
    }
    else {
      if (t.mon == 3) {
        if (t.date > 24 and t.dow == 7) {
          if (t.hour >= 2) {
            DST = 1;
          }
          else {
            DST = 0;
          }
        }
        else {
          if (t.date + (7 - t.dow) > 31) {
            DST = 1;
          }
          else {
            DST = 0;
          }
        }
      }
      else {
        DST = 0;
      }
    }
  }
  else {
    DST = 1;
  }
  return DST;
}  /////////////////////////////////   end of checkDST(t)   ////////////////////////////////////////


//void displayTime1()
//{
//  Time t = RTC.getTime();
//  int myhour;
//  int minutes;
//
//  lcd.setCursor(25, 0);
//
//  if (t.hour + DST == 24) {
//    myhour = 0;
//  }
//  else {
//    myhour = t.hour + DST;
//  }
//  lcd.print(myhour);
//  lcd.print(":");
//  minutes = t.min;
//  if (minutes < 10) {
//    lcd.print("0");
//  }
//  lcd.print(t.min);
//  if (DST) {
//    lcd.print(" *");
//  }
//  else {
//    lcd.print("  ");
//  }
//}   ///////////////////////////// end of displayTime()   ///////////////////////////////////////


void displayTime()
{
  Time t = RTC.getTime();
  int myhour;

  if (t.hour + DST == 24) 
  {
    myhour = 0;
  }
  else 
  {
    myhour = t.hour + DST;
  }
  lcd.printNumI(myhour, 24,0,2);
  lcd.print(":",37,0);

  lcd.printNumI(t.min, 42, 0, 2, '0');
  if (DST) 
  { 
    lcd.print(" *", 54, 0);
  }
  
  
}   ///////////////////////////// end of displayTime2()   ///////////////////////////////////////


void displayWellcome()
{
  lcd.print("Version: ", 0, 0);
  lcd.printNumF(VERSION, 1, 50,0);
  lcd.print("Creado el:",0,16);
  lcd.print(__DATE__, 0,24);
}   ////////////////////////////// end displayWellcome()  //////////////////////////////

void displayError()
{
  lcd.print("Error de",0,0);
  lcd.print("comunicacion",0,8);
} ////////////////////////////// end displayError()  //////////////////////////////


void debugLCD(int i)
{
  lcd.clrRow(5,0,18);
  lcd.printNumI(i,0,40, 2, ' ');
}

void setup()
{
  lcd.InitLCD(65);   // clears the screen and buffer
  //lcd.createChar(DEGREES_CHAR, degrees_glyph);
  lcd.setFont(SmallFont);

  RTC.begin();

  //RTC.setTime(01, 03, 30);  // we can use these lines to set the clock one and the comment it again
  //  RTC.setDate(26,03,2017);
  //  RTC.setDOW(SUNDAY);

  t = RTC.getTime();
  DST = checkDST(t);

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
  //radio.openReadingPipe(1, pipes[1]);
  //radio.startListening();                 // Start listening

  error = true;
  byte i = 0;
  while (error && (i < 20) )
  {
    error = heating(heatingOFF);
    i += 1;
    DEBUG_LCD(i);
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
  lcd.clrScr();
  lcd.print("Temp:", 0,8);
  lcd.print("Cale:",0,16);
  displayTime();
  message = 0;

  roomTemperature = measureTemperature(pin_thermistor);
  batteryVoltage = measureBatteryVoltage(pin_batteryLevel);

  batteryCheckCounter=999;
  
  debounceTimerUP = millis();
  debounceTimerDOWN = millis();

 
 
  
  attachInterrupt(0, wakeUpButtonUp, RISING);
  attachInterrupt(1, wakeUpButtonDown, RISING);
} /////////////////////////////////////////// end setup()  ////////////////////////////////////////////////////

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
    //DEBUG_PRINTLN("Encendiendo calefaccion en [heating]");
  }
  else if (newMode == heatingOFF)
  {
    payload = 0;
    //DEBUG_PRINTLN("Apagando calefaccion en [heating]");
  }

  // Send a message to radio_server
  byte gotByte;
  if (!radio.write( &payload, 1 ))
  {
    //DEBUG_PRINTLN(F("failed."));
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

  //DEBUG_PRINTLN(errorC);

  radio.powerDown();

  if (!errorC)
  {
    DEBUG_PRINTLN("NOT ERROR");
    if (manualModeON)
    {
      currentMode = heatingON;
      if (message != 1)
      {
        lcd.clrRow(4);
        lcd.print("Encend. manual",0,32);
        message = 1;
      }
    }
    else if (manualModeOFF)
    {
      currentMode = heatingOFF;
      if (message != 2)
      {
        lcd.clrRow(4);
        lcd.print("Apagado manual",0,32);
        message = 2;
      }
    }
    else if (autoMode)
    {
      if (newMode == heatingON)
      {
        if (message != 3)
        {
          lcd.clrRow(4);
          lcd.print("Calentando!!",0,32);
          message = 3;
        }
      }
      else if (newMode == heatingOFF)
      {
        if (message != 4)
        {
          lcd.clrRow(4);
          message = 4;
        }
      }
    }
  }
  else
  {
    if (message != 99)
    {
      DEBUG_PRINTLN("ERROR");
      lcd.clrRow(4);
      lcd.print("*** Error ***",0,32);
      message = 99;
    }
  }

  return errorC;
}  //////////////////////////////// end heating(boolean newMode) ///////////////////////////////////////


void loop()
{
//
//  /////////////////  temporal block to show the time spent awake, between sleps  ///////////
//  if (debounceTimerUP>0)
//  {
//    unsigned long runTime = millis() - debounceTimerUP;
//    lcd.clrRow(40);
//    lcd.printNumI(runTime, 40,40);
//  }
//  else if (debounceTimerDOWN>0)
//  {
//    unsigned long runTime = millis() - debounceTimerDOWN;
//    lcd.clrRow(40);
//    lcd.printNumI(runTime, 40,40);
//  }
//  else
//  {
//      unsigned long runTime = millis() - tempCounter;
//      //lcd.clearLine();
//      lcd.clrRow(40);
//      lcd.printNumI(runTime, 40,40);
//  }
//  DEBUG_PRINTLN(debounceTimerUP);
//  DEBUG_PRINTLN(debounceTimerDOWN);
//  DEBUG_PRINTLN(millis());
//  DEBUG_PRINTLN(tempCounter);
//  delay(1000); 
//  //////////////////////////////////////////////////////////////////////////////////////////

  
  if (((millis() - debounceTimerUP) > backlightTimer) && ((millis() - debounceTimerDOWN) > backlightTimer))
  { //////////// Switch off the backlight after (backlightTimer) and go to sleep //////////////////////////////
    digitalWrite(pin_backLight, HIGH); //apaga la luz
    backlightON = false;
    debounceTimerUP = 0;
    debounceTimerDOWN = 0;
    firstButtonPressUP = true;
    firstButtonPressDOWN = true;
    repaintCount = 0;
    loopCount = 0;

//    unsigned long runTime = millis() - tempCounter;
//    DEBUG_PRINT("Awake time= ");
//    DEBUG_PRINTLN(runTime);
//    DEBUG_PRINTLN("Ahora me duermo");
//    DEBUG_PRINTLN(RTC.getTimeStr());
//    delay(200);
//    tempCounter = millis();
    
    int oneMinuteCounter=0;
    for (int ii = 0; ii < sleepTimeRepeats; ii++)
    {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      oneMinuteCounter+=1;
      if (oneMinuteCounter>8 && !standby) // 9*8=72 seconds ellapsed
      {
        displayTime();
        oneMinuteCounter=0;
      }
      if (backlightON)
      {
        displayTime();
        break;
      }
    }
//    runTime = millis() - tempCounter;
//    DEBUG_PRINT("Awake time during sleep time= ");
//    DEBUG_PRINTLN(runTime);
//    DEBUG_PRINT("=======  Justo despues de dormir  =====");
//    DEBUG_PRINTLN(RTC.getTimeStr());
    
    ///////////////////////  temporal variable to show the time spent awake, between sleps  //////////////
//    DEBUG_PRINTLN("============= First place to set tempcounter =============");
//    delay(100);
//    tempCounter = millis();
//    lcd.clrRow(5);
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    
  }   //////////////////////////////////////// end of sleep block  /////////////////////////////////////



  if (standbyUP || standbyDOWN)   ///////////// One button has been pressed  ////////////////////////////
  {
    loopCount = 0;
    detachInterrupt(0);
    detachInterrupt(1);
    standbyUP = false;
    standbyDOWN = false;
    repaintCount = 0;
    DEBUG_PRINTLN("Ahora me despierto ****");
    //    delay(20);
    
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
      DEBUG_PRINTLN("Solo un boton pulsado"); //Solo pulsado un boton
      if (standby)
      {
        lcd.disableSleep();
        DEBUG_PRINTLN("Saliendo de Standby");
        DST = checkDST(t); // Check Daylight Saving Time only after going out from standby
        //lcd.setCursor(0, 4);
        //lcd.clrScr();
        displayTime();
        message=0;
        lcd.print("Temp:",0,8);
        lcd.print("Cale:",0,16);
        lcd.printNumF(setPoint, 1,30,16);
      }
      goingStandby = false;
      standby = false;
    }
  }   ///////////////////////////// end of button press analisys ////////////////////////////////////////


  if (loopCount < 1)
  {
    loopCount += 1;
    DEBUG_PRINTLN("sumo uno a loopCount");
    DEBUG_PRINTLN("dentro de loopcount");
    batteryCheckCounter += 1;

    //  char* timestr = RTC.getTimeStr(FORMAT_SHORT);
    //  char timestr2[6];
    //
    

    //char timestr[] = "00:01";      ////////////////////// used in case not RTC is connected ///////////////

    //  if (timestr[0]== '1' || timestr[0]=='2' || timestr[1]=='9')
    //{
    //    lcd.setPower(true);
    //  }


    //  lcd.setCursor(20,5);
    //  lcd.print(RTC.getTemp());

    //displayTime();

    ////DEBUG_PRINTLN(setPoint);
    //lcd.setCursor(0, 4);
    //lcd.drawBitmap(thermometer, THERMO_WIDTH, THERMO_HEIGHT);

    if (!goingStandby)
    { ////////////////////// normal operation when awake ///////////////////////////
      if (repaintCount < 1)
      {
        DEBUG_PRINTLN("Normal operation when awake. Should happen only once");
        roomTemperature = measureTemperature(pin_thermistor);
        //roomTemperature*=(-1);


        lcd.printNumF(roomTemperature, 1,30,8);


        if (autoMode)
        {
          if (setPoint != old_setPoint)
          {
            lcd.printNumF(setPoint, 1,30,16);
            old_setPoint = setPoint;
          }
        }
        
        if (batteryCheckCounter > batteryChecksLimit)
        {
          DEBUG_PRINTLN("*************   Battery Check....");
          batteryVoltage = measureBatteryVoltage(pin_batteryLevel);
          batteryCheckCounter = 0;
          if (batteryVoltage < batteryLowVoltaje)
          {
            if (!batteryLow)
            {
              batteryLow = true;
              lcd.clrRow(3);
              lcd.print("Bateria baja!", 0,24);
            }
          }
          else
          {
            if (batteryLow)
            {
              batteryLow = false;
              lcd.clrRow(3);
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
              DEBUG_LCD(i);
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
              DEBUG_LCD(i);
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
      if (errorCount!=old_errorCount)
      {
        lcd.printNumI(errorCount, 0, 0);
        old_errorCount+=1;
      }

    }  //////////////////////////////////// end of normal operation when awake //////////////////////////////////////


    else ////////////////////////////////////  if (goingStandby)    ///////////////////////////////////////////////////////////
    { //////////////  in case of both buttons where pressed: switch-off heating (if needed), and powerDown the display
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
          DEBUG_LCD(i);
        }
        if (error)
        {
          lcd.clrScr();
          message=0;
          lcd.print("No pudo apagarse", 0,8);
          lcd.print("la calefaccion.", 0,16);
          lcd.print("Apagar manualmente", 0,24);
          backlightON = false;
          delay(5000);
          lcd.enableSleep();
        }
        else
        {
          DEBUG_PRINTLN("Antes de dormir he apagado la calefaccion. Ahora Standby..");
          currentMode = heatingOFF;
          backlightON = false;
          lcd.clrScr();
          message=0;
          lcd.print("Buenas noches", 5,32);
          delay(3000);
          lcd.enableSleep();
        }
        EIFR = bit (INTF0);  // clear flag for interrupt 0
        EIFR = bit (INTF1);  // clear flag for interrupt 1
        firstButtonPressUP = true;
        firstButtonPressDOWN = true;
        attachInterrupt(0, wakeUpButtonUp, RISING);
        attachInterrupt(1, wakeUpButtonDown, RISING);
        digitalWrite(pin_backLight, HIGH); //apaga la luz
        standby = true;

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
          lcd.clrScr();
          message=0;
          lcd.print("Buenas noches",5,32);
          delay(3000);
          lcd.enableSleep();
          EIFR = bit (INTF0);  // clear flag for interrupt 0
          EIFR = bit (INTF1);  // clear flag for interrupt 1

          firstButtonPressUP = true;
          firstButtonPressDOWN = true;
          attachInterrupt(0, wakeUpButtonUp, RISING);
          attachInterrupt(1, wakeUpButtonDown, RISING);
          digitalWrite(pin_backLight, HIGH); //apaga la luz
          radio.powerDown();
          standby = true;
        }
      }
    }   ///////////////////////////  end of going into deep standby  ////////////////////////////////////////////////

  }
  
} ///////////////////////////////////////   end loop()   ///////////////////////////////////////
