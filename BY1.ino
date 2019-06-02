//Scott Mangiacotti and Celestine
//Brookline, MA USA
//May 2019
//BY1
//Version 1.0

#include <Servo.h>
#include <EEPROM.h>

//Constants
int const GIVE_BACK_TIME = 125;

//Constants for inputs
int const I_POS_ADJUST_PIN = A0;
int const I_ON_OFF = 2;

//Constants for outputs
int const O_RED_LED_PIN = 3;
int const O_GREEN_LED_PIN = 4;
int const O_BLUE_LED_PIN = 5;

int const O_SERVO_PIN = 9;
int const O_BLOWER_PIN = 10;

//Global variables
bool gRunning = false;
bool gSetupMode = false;
bool gTestMode = false;
bool gVerboseDiagMode = false;

Servo gServo;
int gBlowPos;
int gSoapPos;
int gCurrentPos;
int gCurrentPotPos;

int gStepNum = 0;
int gScansInStep;
int gTimeInStep;


//Runs once
void setup() 
{
  //Open a serial port
  Serial.begin(9600);

  //Setup inputs
  pinMode(I_ON_OFF, INPUT);
  
  //Setup digital outputs
  pinMode(O_RED_LED_PIN, OUTPUT);
  pinMode(O_GREEN_LED_PIN, OUTPUT);
  pinMode(O_BLUE_LED_PIN, OUTPUT);
  pinMode(O_BLOWER_PIN, OUTPUT);

  gServo.attach(O_SERVO_PIN);

  //Read settings from NVM
  readSettingsFromNVM();

  //Post product information to serial port
  reportProductInfo();
}


//Runs continuously
void loop()
{
  //Serial port message receipt processing
  if (Serial.available() > 0)
  {
    int iControlCode;
    iControlCode = Serial.parseInt();
    processSerialMessage(iControlCode);
  }

  //Process time before sequencer execution
  gScansInStep++;
  gTimeInStep = gScansInStep * GIVE_BACK_TIME; //in milliseconds

  //Execute sequencer states and transitions
  sequencerExecute();

  //Handle setup mode
  if (gSetupMode == true)
  {
    int iMath;
    iMath = (gCurrentPotPos/1024.0) * 180;
    gServo.write(iMath);
  }

  //Process I/O
  readDevices();
  processLEDs();
  
  //Give a little time back
  delay(GIVE_BACK_TIME);
}


//Execute states and transitions of the sequencer
void sequencerExecute()
{

  if (gStepNum == 0)
  {
    if (gRunning == true)
    {
      gStepNum = 10;
      gScansInStep = 0;
    }

  }
  else if (gStepNum == 10)
  { //go to soap position
    gServo.write(gSoapPos);
    gStepNum = 20;
    gScansInStep = 0;
 
  }
  else if (gStepNum == 20)
  { //servo at soap position
    if (gCurrentPos == gSoapPos)
    {
      gStepNum = 30;
      gScansInStep = 0;
    }
    
  }
  else if (gStepNum == 30)
  { //wait time (0.5 seconds)
    if (gTimeInStep >= 1000)
    {
      gStepNum = 40;
      gScansInStep = 0;
    }
    
  }
  else if (gStepNum == 40)
  { //go to blow position
    gServo.write(gBlowPos);
    gStepNum = 50;
    gScansInStep = 0;
    
  }
  else if (gStepNum == 50)
  { //servo at blow position
    if (gCurrentPos == gBlowPos)
    {
      gStepNum = 60;
      gScansInStep = 0;
    }
    
  }
  else if (gStepNum == 60)
  { //turn on blower
    digitalWrite(O_BLOWER_PIN, HIGH);
    gStepNum = 70;
    gScansInStep = 0;
    
  }
  else if (gStepNum == 70)
  { //wait time (0.5 seconds)
    if (gTimeInStep >= 2000)
    {
      gStepNum = 80;
      gScansInStep = 0;
    }
    
  }
  else if (gStepNum == 80)
  { //turn off blower
    digitalWrite(O_BLOWER_PIN, LOW);
    gStepNum = 99;
    gScansInStep = 0;
    
  }
  else if (gStepNum == 99)
  { //return to start
    gStepNum = 0;
    gScansInStep = 0;
  }
  
}

//Read positional values into global variables
void readDevices()
{
  //Read potentiomter value (scaled 0-1023)
  gCurrentPotPos = analogRead(I_POS_ADJUST_PIN);

  //Read on/off toggle switch position
  int iVal;
  iVal = digitalRead(I_ON_OFF);
  if (iVal == 0)
  {
    gRunning = false;
  }
  else if (iVal == 1)
  {
    gRunning = true;
  }

  //Read servo actual position
  gCurrentPos = gServo.read();

  //Post status
  if (gVerboseDiagMode == true)
  {
    displayStatus();
  }

}

void processLEDs()
{
  //Turn on red LED
  if (gRunning == false && gSetupMode == false)
  {
    digitalWrite(O_RED_LED_PIN, HIGH);
    digitalWrite(O_GREEN_LED_PIN, LOW);
    digitalWrite(O_BLUE_LED_PIN, LOW);
  }
  
  //Turn on green LED
  if (gRunning == true)
  {
    digitalWrite(O_RED_LED_PIN, LOW);
    digitalWrite(O_GREEN_LED_PIN, HIGH);
    digitalWrite(O_BLUE_LED_PIN, LOW);
  }

  //Turn on blue LED
  if (gSetupMode == true)
  {
    digitalWrite(O_RED_LED_PIN, LOW);
    digitalWrite(O_GREEN_LED_PIN, LOW);
    digitalWrite(O_BLUE_LED_PIN, HIGH);
  }

}

//Display status of system to serial port
void displayStatus()
{

  Serial.print("step num: ");
  Serial.println(gStepNum);
  
  Serial.print("scans in step: ");
  Serial.println(gScansInStep);

  Serial.print("time in step (ms): ");
  Serial.println(gTimeInStep);

  Serial.print("servo position: ");
  Serial.println(gCurrentPos);

  Serial.print("potentiometer pos: ");
  Serial.println(gCurrentPotPos);

  Serial.println("---");
  
}


//Read values from EEPROM for soap position and blow position of the servo
void readSettingsFromNVM()
{
  int iAddr;
  int iVal;

  //Initialize address counters
  iAddr = 0;
  iVal = 0;

  //Read soap servo position from non-volatile-memory
  EEPROM.get(iAddr, iVal);

  //Validate value
  if (iVal >= 0 && iVal <= 180)
  {
    gSoapPos = iVal;
    Serial.print("Soap position successfully read from NVM: ");
    Serial.println(gSoapPos);
    
  }
  else
  {
    gSoapPos = 45;
    Serial.print("Failure to read soap position from NVM: ");
    Serial.print(iVal);
    Serial.print(", value set to: ");
    Serial.println(gSoapPos);
    
  }

  //Increment address counter
  iAddr += sizeof(int);
  iVal = 0;

  //Read blower servo position from non-volatile-memory
  EEPROM.get(iAddr, iVal);

  if (iVal >= 0 && iVal <= 180)
  {
    gBlowPos = iVal;
    Serial.print("Blower position successfully read from NVM: ");
    Serial.println(gBlowPos);
    
  }
  else
  {
    gBlowPos = 135;
    Serial.print("Failure to read blower position from NVM: ");
    Serial.println(iVal);
    Serial.print(", value set to: ");
    Serial.println(gBlowPos);
    
  }
  
}


//Write values to EEPROM for blower and soap servo positions
void writeSettingsToNVM()
{
  int iAddr;

  //Write red distance to non-volatile-memory
  iAddr = 0;
  EEPROM.put(iAddr, gSoapPos);

  //Write yellow distance to non-volatile-memory
  iAddr += sizeof(int);
  EEPROM.put(iAddr, gBlowPos);

  //Post results
  Serial.println("settings successfully saved to NVM");
}


//Process received messages from the serial port interface
//Input parameter iControlCode is the value received from the serial port to be processed
//First two digits are the control command, remaining three are the value to process
void processSerialMessage(int iControlCode)
{
  int iControlCommand;
  int iControlValue;
  
  //Calculate command and value
  iControlCommand = iControlCode / 1000;
  iControlValue = iControlCode % 1000;

  //Report command and value
  Serial.print("control code: ");
  Serial.println(iControlCode);
  
  Serial.print("control command: ");
  Serial.println(iControlCommand);
  
  Serial.print("control value: ");
  Serial.println(iControlValue);

  //Misc command category
  if (iControlCommand == 10)
  {
    if (iControlValue == 0)
    { //display app info
      reportProductInfo();
      
    }
    else if (iControlValue == 1)
    { //verbose messaging toggle
      if (gVerboseDiagMode == false)
      {
        gVerboseDiagMode = true;
        Serial.println("verbose messaging enabled");
      }
      else
      {
        gVerboseDiagMode = false;
        Serial.println("verbose messaging disabled");
      }
      
    }
    else if (iControlValue == 2)
    { //enter setup mode
      if (gStepNum == 0 && gRunning == false)
      {
        gSetupMode = true;
        Serial.println("Now in setup mode");
      }
      else
      {
        Serial.println("Could not enter setup mode because system is running");
      }

    }
    else if (iControlValue == 3)
    { //exit setup mode
      gSetupMode = false;
      Serial.println("Setup mode exited");

    }
    else if (iControlValue == 4)
    { //save new soap position
      if (gSetupMode == true)
      {
        gSoapPos = (gCurrentPotPos/1024.0) * 180.0;
        writeSettingsToNVM();
        Serial.print("New soap position saved: ");
        Serial.println(gSoapPos);
      }
      else
      {
        Serial.println("Cannot save new soap position when not in setup mode");
      }
      
    }
    else if (iControlValue == 5)
    { //save new blower position
      if (gSetupMode == true)
      {
        gBlowPos = (gCurrentPotPos/1024.0) * 180.0;
        writeSettingsToNVM();
        Serial.print("New blower position saved: ");
        Serial.println(gBlowPos);
      }
      else
      {
        Serial.println("Cannot save new soap position when not in setup mode");
      }
      
    }
    else if (iControlValue == 6)
    { //display soap position
      Serial.print("Current soap position: ");
      Serial.println(gSoapPos);

    }
    else if (iControlValue == 7)
    { //display blower position
      Serial.print("Current blower position: ");
      Serial.println(gBlowPos);

    }
    else if (iControlValue == 8)
    { //display servo position
      Serial.print("Current servo position: ");
      Serial.println(gCurrentPos);

    }
    else if (iControlValue == 9)
    { //display potentiometer position
      Serial.print("Current potentiometer position: ");
      Serial.println(gCurrentPotPos);      

    }
    else if (iControlValue == 10)
    { //enter test mode
      if (gStepNum == 0 && gRunning == false && gSetupMode == false)
      {
        gTestMode = true;
        Serial.println("Now in test mode");
      }
      else
      {
        Serial.println("Could not enter test mode because system is running or in setup mode");
      }

    }
    else if (iControlValue == 11)
    { //exit test mode
      gTestMode = false;
      Serial.println("Test mode exited");
      
    }
    else if (iControlValue == 12)
    { //test mode: read on/off sw state
      if (gTestMode == true)
      {
        int iRead;
        iRead = digitalRead(I_ON_OFF);
        if (iRead == 0)
        {
          Serial.println("Test mode: Switch is off");
        }
        else if (iRead == 1)
        {
          Serial.println("Test mode: Switch is on");
        }
        
      }
      else
      {
        Serial.println("Must be in test mode");
        
      }
      
    }
    else if (iControlValue == 13)
    { //cycle red LED
      if (gTestMode == true)
      {
        digitalWrite(O_RED_LED_PIN, HIGH);
        delay(1000);
        digitalWrite(O_RED_LED_PIN, LOW);
      }
      else
      {
        Serial.println("Must be in test mode");
      }
      
    }
    else if (iControlValue == 14)
    { //cycle green LED
      if (gTestMode == true)
      {
        digitalWrite(O_GREEN_LED_PIN, HIGH);
        delay(1000);
        digitalWrite(O_GREEN_LED_PIN, LOW);
      }
      else
      {
        Serial.println("Must be in test mode");
      }      

    }
    else if (iControlValue == 15)
    { //cycle blue LED
      if (gTestMode == true)
      {
        digitalWrite(O_BLUE_LED_PIN, HIGH);
        delay(1000);
        digitalWrite(O_BLUE_LED_PIN, LOW);
      }
      else
      {
        Serial.println("Must be in test mode");
      }
      
    }
    else if (iControlValue == 16)
    { //cycle blower
      if (gTestMode == true)
      {
        digitalWrite(O_BLOWER_PIN, HIGH);
        delay(5000);
        digitalWrite(O_BLOWER_PIN, LOW);
      }
      else
      {
        Serial.println("Must be in test mode");
      }
    }
    else if (iControlValue == 17)
    {
      if (gSetupMode == false && gTestMode == false)
      {
        gRunning = true;
        Serial.println("Run mode started");
      }
      else
      {
        Serial.println("Could not enter run mode because system is in setup mode or test mode");
      }
    }
    else if (iControlValue == 18)
    {
      gRunning = false;
      Serial.println("Run mode stopped");
    }
    else
    {
      Serial.print("invalid control value: ");
      Serial.println(iControlValue);
    }
  }

  if (iControlCommand == 11)
  { //test mode: servo position command
    if (iControlValue >= 0 || iControlValue <= 180)
    {
      gServo.write(iControlValue);
      Serial.print("servo commanded to test mode position: ");
      Serial.println(iControlValue);
    }

  }

  if (iControlCommand == 12)
  {
    //spare
  }

  //End of request string
  Serial.println("-----");
}


//Send product information to the serial port
void reportProductInfo()
{
  //Report product and other information to serial port
  Serial.println("BY1");
  Serial.println("by Scott Mangiacotti and Celestine");
  Serial.println("Brookline, MA USA");
  Serial.println("May 2019");
}
