#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SSD1306_128_32
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
#define BATT_MAX 12.6   // Max Charge Voltage. 
#define BATT_MIN 9.6    // Minimum "safe" Battery Voltage - Used to determine Highest power setting voltage.
#define ABSOLUTE_MIN 5  // Used to determine lowest power setting voltage.
#define ANALOG_MAX 830  // This should be the analog reading of BATT_MAX voltage, can vary on voltage divider or battery.  
#define ANALOG_MIN 630  // Same as about but applies to BATT_MIN voltage.
#define ARRAY_SIZE 16   // Number of array elements used for battery voltage tracking. This has downsteam effects on visualizations.

// PinMode vars
const int buttonPin = 9;  // Main button
const int ledPin = 11;    // Button LED
const int relayPin = 10;  // Gate control
const int vBatt = A0;     // Battery voltage after divider

// Button de-bounce vars           
bool lastButtonState = LOW; 
unsigned long lastDebounceTime = 0;  
unsigned char debounceDelay = 25;

// Battery data vars
short unsigned int battVoltage[ARRAY_SIZE]; // Holds battery voltage readings
short unsigned int sagVoltage = 680; // Holds lowest battery voltage reading 
short unsigned int setPoint_External = 1; // Used for UI
float setPoints_Internal[10];
float setPoint_Internal = ((setPoint_External) - 1) * ((BATT_MIN / BATT_MAX) - (ABSOLUTE_MIN / BATT_MAX)) / (10 - 1) + (ABSOLUTE_MIN / BATT_MAX); // internal use maps the 1 - 10 range to percentage 
short unsigned int resolution = 0;// resault of display width and array size used for screen visuals
int lastPwmVal; // Holds previous calculated PWM value, used for filtering
int tuner = 0;

// Menu and display vars
short unsigned int graphHigh = 0; // Holds Highest value in array, use for scaling
short unsigned int graphLow = ANALOG_MAX; // Holds Lowest value in array, use for scaling
int fontSize = display.height() / 32; // Actually adjusting font size for screen size doesnt work--- so this is currently broken
int battPercent = 0; // Holds battery life percentage, used for battery display 
int visualization = 1; // Holds current visualization
const char numOfVisualization = 2; // Holds total number of visualisations
int stealth = 0; // bool to toggler "stealth" mode
bool powerSource = false; // false = battery, true = mains

typedef struct{  
  int pin;
  int buttonCounterTimeOut;
  bool waiting;  
  bool pressed;
  int sequence;
  unsigned long buttonCounterTimer;
}button;// typeFlag = 0

typedef struct{
  int interval; 
  unsigned long timer; 
  unsigned long previous;
  bool waiting;
}timer;// typeFlag = 1

typedef struct{  
  int val[ARRAY_SIZE];
  int updateInterval;
}sensor;// typeFlag = 2

typedef struct{  
  int currentState;
  int numberOfStates;
}menu;// typeFlag = 3

menu mainMenu = {.currentState = 1, .numberOfStates = 9};
menu settingsMenu = {.currentState = 1, .numberOfStates = 4};
button mainButton = {.pin = 9, .buttonCounterTimeOut = 500};
timer mainButtonDebounceTimer = {.interval = 25};
timer buttonHoldIncrementTimer = {.interval = 200};
timer mainButtonSequenceTimer = {.interval = 300};
timer lazyVoltageCheckTimer = {.interval = 500};
timer crazyVoltageCheckTimer = {.interval = 2};
timer screenUpdateTimer = {.interval = 16};

// screen layout vars - split into 8(2col 4 row) sectors each with an X and Y var
unsigned short int S1_X = 0;
unsigned short int S1_Y = 0;

unsigned short int S2_X = display.width() / 2 + 1;
unsigned short int S2_Y = 0;

unsigned short int S3_X = 0;
unsigned short int S3_Y = display.height() / 4 + 1;

unsigned short int S4_X = display.width() / 2 + 1;
unsigned short int S4_Y = display.height() / 4 + 1;

unsigned short int S5_X = 0;
unsigned short int S5_Y = ((display.height() / 4 ) * 2) + 1;

unsigned short int S6_X = display.width() / 2 + 1;
unsigned short int S6_Y = ((display.height() / 4 ) * 2) + 1;

unsigned short int S7_X = 0;
unsigned short int S7_Y = ((display.height() / 4 ) * 3) + 1;

unsigned short int S8_X = display.width() / 2 + 1;
unsigned short int S8_Y = ((display.height() / 4 ) * 3) + 1;

// Battery indicator bitmaps h = 8 w = 16
static const unsigned char PROGMEM battBar1[] =
{ 
  B00000000, B00000000,
  B00000000, B00000000,
  B00111111, B11111111,
  B00111111, B11111110,
  B00111111, B11111100,
  B00111111, B11111000,
  B00000000, B00000000,
  B00000000, B00000000, 
};
static const unsigned char PROGMEM battBar2[] =
{ 
  B00000000, B00000000,
  B00000000, B00000000,
  B00001111, B11111111,
  B00011111, B11111110,
  B00111111, B11111100,
  B01111111, B11111000,
  B00000000, B00000000,
  B00000000, B00000000, 
};
static const unsigned char PROGMEM battBar3[] =
{ 
  B00000000, B00000000,
  B00000000, B00000000,
  B00001111, B11111000,
  B00011111, B11111100,
  B00111111, B11111100,
  B01111111, B11111000,
  B00000000, B00000000,
  B00000000, B00000000, 
};
static const unsigned char PROGMEM battOutline[] =
{ 
  B01111111, B11111111, B11111111, B11111111, B11111111, B11111100,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000100,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000011,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000011,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000011,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000011,
  B10000000, B00000000, B00000000, B00000000, B00000000, B00000100,
  B01111111, B11111111, B11111111, B11111111, B11111111, B11111100,
};
static const unsigned char PROGMEM dynaLogo[] =
{
  B11110011, B10011000, B01001100, B01100111, B11000000,
  B11111001, B00011000, B11001100, B01100011, B11100000,
  B11001000, B00011101, B10000100, B01000010, B00100000,
  B11001100, B00110101, B10000110, B11000010, B00110000,
  B11000100, B00110111, B00000010, B10000001, B11110000,
  B11000110, B01100111, B00110011, B10011001, B10000000,
  B11111100, B01100111, B00110001, B00011001, B10000000,
  B11111100, B11000010, B01111001, B00111100, B10000000,
};
void setup() {
  Serial.begin(9600);
  // set-up pins
  pinMode(mainButton.pin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(vBatt, INPUT);

  // set-up display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  display.display();
  display.clearDisplay();
  delay(500);

  //fill vbatt array
  for(int i = 0; i < ARRAY_SIZE; i++){
    battVoltage[i] = analogRead(vBatt);
    delay(10);
  }
}

void loop() {
  display.clearDisplay();
  button* temp = checkButton(&mainButton);
  while(stealth > 0){
    button* temp = checkButton(&mainButton);   
    if(temp->waiting == false){
      if(temp->pressed == true){
        if(incrementTimer(&crazyVoltageCheckTimer) == true){
              voltageTest(temp->pressed);
            }
        analogWrite(relayPin,getPWM(battVoltage[ARRAY_SIZE - 1]));
      }else{
        analogWrite(relayPin,0);
      }
      if(temp->sequence == 3){
        stealth = 0;
        Serial.println(temp->sequence);
        Serial.println(stealth);
      }
      if(incrementTimer(&lazyVoltageCheckTimer) == true){
        voltageTest(temp->pressed);
      }
    }
  }
  
  if(mainButton.waiting == false){
    switch(mainMenu.currentState){
      case 1://Main screen
        updateMenu(&mainButton, &mainMenu, 1, 1);
        if(mainButton.pressed == true){
          analogWrite(relayPin,getPWM(battVoltage[ARRAY_SIZE - 1]));
          digitalWrite(ledPin,HIGH);
          if(incrementTimer(&crazyVoltageCheckTimer) == true){
            voltageTest(temp->pressed);
          }
        }else{
          if(incrementTimer(&lazyVoltageCheckTimer) == true){
            voltageTest(temp->pressed);
          }
          analogWrite(relayPin,0);
        }
        visual(visualization, 0, 8, display.width(), display.height());
        infoBar();
        break;
        
      case 2://Power adjust screen
        updateMenu(&mainButton, &mainMenu, 1, 1);
        if(temp->pressed == true){
          if(incrementTimer(&buttonHoldIncrementTimer) == true){
            setPoint_External++;
            if(setPoint_External >= 2){
            setPoint_Internal = ((setPoint_External) - 1) * ((BATT_MIN / BATT_MAX) - (ABSOLUTE_MIN / BATT_MAX)) / (10 - 1) + (ABSOLUTE_MIN / BATT_MAX);
            }else{
              setPoint_Internal = 0.397;
            }
            if(setPoint_External >= 11){
              setPoint_External = 1;              
            }
          }
        }        
        powerLevelIndicator();
        infoBar();
        break;       
      case 3://Setting menu selection screen
        updateMenu(&mainButton, &mainMenu, settingsMenu.currentState, 1);
        writeText(3, 0, 1, " Visual", 0, 0, false);
        writeText(4, 0, 1, " Power Src", 0, 0, false);
        writeText(5, 0, 1, " Stats", 0, 0, false);
        writeText(6, 0, 1, " Stealth", 0, 0, false);
        writeText(7, 0, 1, " About", 0, 0, false);
        if(temp->pressed == true){
          if(incrementTimer(&buttonHoldIncrementTimer) == true){
            settingsMenu.currentState++;
            if(settingsMenu.currentState >= 6){
              settingsMenu.currentState = 1;
            }
          }
        }       
        switch(settingsMenu.currentState){
          case 1:
            writeText(3, 0, 1, " Visual", 0, 0, true);
            break;
            
          case 2:
            writeText(4, 0, 1, " Power Src", 0, 0, true);
            break;
            
          case 3:
            writeText(5, 0, 1, " Stats", 0, 0, true);
            break;
            
          case 4:
            writeText(6, 0, 1, " Stealth", 0, 0, true);    
            break;
            
          case 5:
            writeText(7, 0, 1, " About", 0, 0, true);
            break;
            
          case 6:
            break;
            
        }  
        infoBar();
        break;
        
      case 4:// Visual selection screen
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState);
        writeText(5, 0, 1, " Visual", 0, 0, false);
        writeText(6, 0, 1, "null", visualization, 0, false);
        if(temp->pressed == true){
          if(incrementTimer(&buttonHoldIncrementTimer) == true){
            visualization++;
            if(visualization >= 3){
              visualization = 1;
            }
          }
        }
        infoBar();
        break;
        
      case 5:// Power source selection screen
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState);
        writeText(5, 0, 1, " Power src", 0, 0, false);
        infoBar();
        break;
        
      case 6:// Stat setting Screen
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState);
        writeText(5, 0, 1, " Stat", 0, 0, false);
        infoBar();
        break;
        
      case 7:// Stealth Settings
        infoBar();    
        writeText(5, 0, 1, " Stealth", 0, 0, false);
          if(temp->sequence == 3){
              stealth = 1;
              display.clearDisplay();
              mainMenu.currentState = 1;
          }
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState); 
        
        break;
              
      case 8:// Something IDK yet
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState);
        writeText(5, 0, 1, " Something", 0, 0, false);
        infoBar();
        break;
        
      case 9:// About screen (jokes)
        updateMenu(&mainButton, &mainMenu, 0, settingsMenu.currentState);
        writeText(5, 0, 1, " About", 0, 0, false);
        infoBar();
        break;
    }  
   display.display(); 
  }    
}

//-------------------------------------------------------------------------------------------------------------------------------

void voltageTest(bool flag){
  int newVoltage = analogRead(vBatt);
  float TF = .5;
  if(flag == 0){
    battPercent = map(newVoltage, ANALOG_MIN, ANALOG_MAX, 0, 100);
    battPercent = constrain(battPercent, 0, 100);
  }else{
    graphHigh = 0;
    graphLow = 1028;
    for(int i = 0; i < ARRAY_SIZE - 1; i++){
      battVoltage[i] = battVoltage[i + 1];
      if(battVoltage[i] > graphHigh){
        graphHigh = battVoltage[i];
      }else if(battVoltage[i] < graphLow){
        graphLow = battVoltage[i];
      }   
    }
    newVoltage = constrain(TF * newVoltage + (1 - TF) * battVoltage[ARRAY_SIZE - 1], ANALOG_MIN, ANALOG_MAX); 
    battVoltage[ARRAY_SIZE - 1] = newVoltage;
    if(battVoltage[ARRAY_SIZE - 1] > graphHigh){
      graphHigh = battVoltage[ARRAY_SIZE - 1];
    }else if(battVoltage[ARRAY_SIZE - 1] < graphLow){
      graphLow = battVoltage[ARRAY_SIZE - 1];
    }       
  }
}

//--------------------------------------------------------------------------------------------------------------------------
int getPWM(int vBatt){
  float TF = .5; 
  if (vBatt < sagVoltage){
    sagVoltage = vBatt;
  }
  int pwmMaths = (((setPoint_Internal * ANALOG_MAX) / vBatt) * 255) + tuner;
  pwmMaths = constrain(TF * pwmMaths + (1 - TF) * lastPwmVal, 1, 255);
  lastPwmVal = pwmMaths;
  return pwmMaths;
}
// int pwmMaths = ((setPoint_Internal * ANALOG_MAX) / newVoltage ) * 255;
//pwmMaths = ((6 * 680) / x) * 255;
//    pwmMaths = constrain(TF2 * pwmMaths + (1 - TF2) * pwmVals[ARRAY_SIZE - 1], 1, 255);
//---------------------------------------------------------------------------------------------------------------------------------
void visual(int mode,int xPos, int yPos, int width, int height){
  
  if(mode == 1){
    width -= 24;
    height -= height / 4;
    int resolution = width / ARRAY_SIZE;
    for(int i = 0; i <= ARRAY_SIZE - 2; i ++){
      int temp = map(battVoltage[i], graphLow, graphHigh, height - 1, 0);
      int temp2 = map(battVoltage[i + 1], graphLow, graphHigh, height - 1, 0);
      display.drawLine(i * resolution + xPos, temp + yPos, (i * resolution) + resolution + xPos, temp2 + yPos, WHITE);    
    }
    writeText(4, 36, 1, "float", 0, BATT_MAX * setPoint_Internal, false);
    writeText(6, 36, 1, "float", 0, (((float)graphHigh - (float)graphLow)/((float)ANALOG_MAX - (float)ANALOG_MIN)) * (float)BATT_MAX, false);
    writeText(8, 36, 1, "float", 0, ((float)battVoltage[ARRAY_SIZE - 1] / (float)ANALOG_MAX) * (float)BATT_MAX, false);   
  }else if(mode == 2){
    int j = 0;
    int resolution = width / ARRAY_SIZE;
    for(int i = 0; i <= width - 1; i ++){
      if(i > 0 && i % resolution == 0){
        j++;
      }
      int temp = map(battVoltage[j], graphLow, graphHigh, height - 1, height / 4);
      int temp2 = (height / 4) + ((height - (height / 4))/2) + (temp / 4) * sin((i * PI) / resolution);
      display.drawPixel(i, temp2, WHITE);

//      temp2 = (display.height() / 4) + ((display.height() - (display.height() / 4))/2) + -(temp / 4) * sin((i * PI) / resolution);
//      display.drawPixel(i, temp2, WHITE);
    } 
  }
}

//-------------------------------------------------------------------------------------------------------------------------------------------
void writeText(int sector, int offSet, int fontSize, String text, int var, float floatVar, bool invert){
  char Xpos = 0;
  char Ypos = 0;
  switch(sector){
    case 1:
      Xpos = S1_X;
      Ypos = S1_Y;
      break;
      
    case 2:
      Xpos = S2_X;
      Ypos = S2_Y;
      break;
      
    case 3:
      Xpos = S3_X;
      Ypos = S3_Y;
      break;
    case 4:
      Xpos = S4_X;
      Ypos = S4_Y;
      break;
      
    case 5:
      Xpos = S5_X;
      Ypos = S5_Y;
      break;
      
    case 6:
      Xpos = S6_X;
      Ypos = S6_Y;
      break;
    
    case 7:
      Xpos = S7_X;
      Ypos = S7_Y;
      break;
      
    case 8:
      Xpos = S8_X;
      Ypos = S8_Y;
      break;     
  }
  if(invert == false){
    display.setTextColor(WHITE);
    if(text.equals("null") == true){
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(var);
    }else if(text.equals("float") == true){
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(floatVar, 1);
    }else{
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(text);
    }   
  }else{
    display.setTextColor(BLACK);
    display.fillRoundRect(Xpos + offSet, Ypos, display.width()/2, display.height() / 4 , (display.height() / 4) / 2 , WHITE);
    if(text.equals("null") == true){
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(var);
    }else if(text.equals("float") == true){
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(floatVar, 1);
    }else{
      display.setTextSize(fontSize);
      display.setCursor(Xpos + offSet, Ypos);
      display.print(text);
    } 
  }
}

//----------------------------------------------------------------------------------------------------------------
bool incrementTimer(timer* timerToCheck){
  timerToCheck->timer = millis();
    if (timerToCheck->timer - timerToCheck->previous >= timerToCheck->interval){
      timerToCheck->previous = timerToCheck->timer;
      if (timerToCheck->waiting == LOW) {
        timerToCheck->waiting = HIGH;
        return true;
      } else {
        timerToCheck->waiting = LOW;  
      }
    } 
  return false;
}

//------------------------------------------------------------------------------------------------------------
int checkButton(button* buttonToCheck){
  int reading = digitalRead(buttonToCheck->pin);
  if(reading != lastButtonState){
    lastDebounceTime = millis();   
  }
  if((millis() - lastDebounceTime) > debounceDelay){
    if (reading != buttonToCheck->pressed){
      buttonToCheck->pressed = reading;
      if (buttonToCheck->pressed == HIGH){    
        buttonToCheck->sequence++;
        buttonToCheck->pressed = true;
        buttonToCheck->waiting = true;
        buttonToCheck->buttonCounterTimer = millis();       
      }else{
        buttonToCheck->pressed = false;
      }
    }
  } 
  if((millis() - buttonToCheck->buttonCounterTimer) > buttonToCheck->buttonCounterTimeOut){
    buttonToCheck->waiting = false;
    if(buttonToCheck->sequence > 3){
      buttonToCheck->sequence = 3;
    }
  }  
  lastButtonState = reading; 
  return buttonToCheck;
}

//-----------------------------------------------------------------------------------------------------------------
int  updateMenu(button* buttonToCheck, menu* menuToUpdate, int intervalUp, int intervalDown){
  switch(buttonToCheck->sequence){
    case 1:
      break;  
    case 2:
      menuToUpdate->currentState += (intervalDown * -1);
      if(menuToUpdate->currentState <= 0){
        menuToUpdate->currentState = 1;
      }
      break;
    case 3:
      menuToUpdate->currentState += intervalUp;
      if(menuToUpdate->currentState >= menuToUpdate->numberOfStates + 1){
        menuToUpdate->currentState = menuToUpdate->numberOfStates;
      }
      break;
    default:
      break;
  }
  buttonToCheck->sequence = 0;
  return menuToUpdate;
}

//-----------------------------------------------------------------------------------------------------------------
void infoBar(){
  display.setTextColor(WHITE); 
  display.drawBitmap(S1_X, S1_Y, battOutline, 48, 8, WHITE);
  if(battPercent >= 25){ 
    display.drawBitmap(S1_X + 2, S1_Y, battBar1, 16, 8, WHITE);
      if(battPercent > 50){
        display.drawBitmap(S1_X + 16, S1_Y, battBar2, 16, 8, WHITE);
          if(battPercent > 75){
            display.drawBitmap(S1_X + 30, S1_Y, battBar3, 16, 8, WHITE);
          }
      }
  }   
  if(battPercent < 10){
    display.setTextSize(1);
    display.setCursor(3,0);
    display.print("LowBatt");
  }
  display.setTextSize(1.5);
  display.setCursor(S2_X,S2_Y);
  display.print("Power =");
  display.setCursor(S2_X + 48 ,S2_Y);
  display.print(setPoint_External);
}

//-----------------------------------------------------------------------------------------------------------------
void powerLevelIndicator(){
  int colXStart = ((display.width() / 2) - (((display.width() - 10) / 10) / 2)) - ((setPoint_External * (display.width() - 10) / 10)/2) ;
  for(int i = 1; i <= setPoint_External; i++){
    int colWidth = (display.width() - 10) / 10;
    int colHeight = (i * 2) + (display.height() / 8);
    int colX = colXStart + (i * colWidth);
    int colY = ((display.height() / 2) + 3) - (colHeight / 2);
    display.drawRoundRect(colX , colY, colWidth, colHeight, colWidth / 2, WHITE);
  }
}

//-----------------------------------------------------------------------------------------------------------------
