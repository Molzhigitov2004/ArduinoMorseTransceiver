#include <RF24.h>//Lib for NRF24L01
#include <Wire.h>//Lin for I2C LCD
#include <SPI.h>//Lib for NRF24L01
#include <nRF24L01.h>//Lib for NRF24L01
#include <LiquidCrystal_I2C.h>//Lin for I2C LCD

const int MORSE_TABLE_SIZE = 36;//Sise of encoded characters array
const char* morseTable[MORSE_TABLE_SIZE] = {//Morse codes for A-Z 0-9
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..", 
    "-----", ".----", "..---", "...--", "....-", ".....", 
    "-....", "--...", "---..", "----."
};
const char charTable[MORSE_TABLE_SIZE] = {//Symbols for A-Z 0-9
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', 
    '6', '7', '8', '9'
};

const int TIME_UNIT = 200;//Basic time interval equivalent to dot length
#define BUTTON_PIN 3
#define BUZZER_PIN 2
#define LED_PIN 10
#define MODE_PIN 4
LiquidCrystal_I2C lcd(0x27, 16, 2);//LCD setup, address 0x27, 16x2 display
RF24 radio(7, 8);//RF24, CE pin 7, CSN pin 8
const byte address[6] = "00001";//Address for NRF24L01 communication
const int debounceDelay = 50;//Debounce delay


char morseWord[100];//This char array stores morse code for a word in format(".../---/.../")
char convertedWord[100];//This char array stores decoded word ("SOS")
// "/" is a divisor between letters

bool buttonPressed = false;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressTime = 0;
unsigned long lastReleaseTime = 0;
bool spacePrinted = false;

void setup(){
  Serial.begin(9600);
  //LCD initialization sequence with greetings
  lcd.init();
  lcd.backlight();
  lcd.print("MorseTransceiver");
  delay(1500);
  lcd.clear();
  lcd.print("Mark 1.0");
  delay(1500);
  //pinModes for BUTTON, BUZZER, LED, SWITCH
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  pinMode(LED_PIN, OUTPUT);
  pinMode(MODE_PIN, INPUT_PULLUP);

  radio.begin();//Start NRF24L01
  radio.setPALevel(RF24_PA_MAX);//Set MAX power level for max range
}

void loop(){//basically checks state of switch and runs accordingly either transmitter or receiver
  bool currentModeState = digitalRead(MODE_PIN);
  //Serial.println(currentModeState);
    if(currentModeState == 1){
      radio.openWritingPipe(address);
      radio.stopListening();
      lcd.clear();
      lcd.print("Enter morse...");
      //Serial.println("Switching to Transmitter mode");
      runTransmitter();
    }else{
      radio.openReadingPipe(0, address);
      radio.startListening();
      lcd.clear();
      lcd.print("Listening...");
      //Serial.println("Switching to Receiver mode");
      runReceiver();
    }
}

void runTransmitter(){
  detectMorseInput();//function to detect button input
  unsigned long idleTime = millis() - lastReleaseTime;//Calculates idle time
  if(lastReleaseTime != 0 && idleTime > 5 * TIME_UNIT && !spacePrinted){//basically idle time interval to separate letters
    addMorseSymbol('/');
    spacePrinted = true;
  }
  if(lastReleaseTime != 0 && idleTime > 10 * TIME_UNIT){//idle time for sending word
    //Serial.println("sent data");
    //Serial.println(morseWord);
    bool success = radio.write(&morseWord, sizeof(morseWord));
    morseToText();//function that decodes Morse code
    printLCD(LOW);
    if(success){
      Serial.println("Message Sent Successfully!");
    }else{
      Serial.println("Message Failed to Send!");
    }
    lastReleaseTime = 0;
    spacePrinted = false;
    convertedWord[0] = '\0';//clear buffer array for converted word
  }
}

// Receiver Mode Function
void runReceiver(){
  if(radio.available()){
    radio.read(&morseWord, sizeof(morseWord));//gets data to morseWord
    fancyEffects();//this function does fancy lighting and beeping like in the movies
    Serial.print("Received Morse: ");
    Serial.println(morseWord);
    morseToText();
    printLCD(HIGH);
    convertedWord[0] = '\0';//clear buffer array for converted word
  }
}

void detectMorseInput(){//debouncing with some fancy sound and time intervals for . and -
  int reading = digitalRead(BUTTON_PIN);

  if(reading != lastButtonState){
    lastDebounceTime = millis();
  }

  if((millis() - lastDebounceTime) > debounceDelay){
    if(reading == LOW && !buttonPressed){
      buttonPressed = true;
      buttonPressTime = millis();
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
    }else if (reading == HIGH && buttonPressed){
      buttonPressed = false;
      unsigned long pressDuration = millis() - buttonPressTime;//measures press duration for . and -
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, LOW);

      if(pressDuration < TIME_UNIT){ according to pressDuration pushes either - or .
        addMorseSymbol('.');
      }else{
        addMorseSymbol('-');
      }

      lastReleaseTime = millis();
      spacePrinted = false;
    }
  }

  lastButtonState = reading;
}



void addMorseSymbol(char symbol){//just aux function that appends symbol
  int len = strlen(morseWord);
  if(len < sizeof(morseWord) - 1){
    morseWord[len] = symbol;
    morseWord[len + 1] = '\0';
    Serial.print(symbol);
  }
}

void morseToText() {//decodes Morse code
  char morseLetter[10] = "";
  int len = strlen(morseWord);

  for(int i = 0; i < len; i++){
    if(morseWord[i] == '/'){// "/" means end of the letter
      morseToText_helper(morseLetter);//decodes letter and appends it
      morseLetter[0] = '\0';//clear buffer
    }else{
      int mlen = strlen(morseLetter);
      if(mlen < sizeof(morseLetter) - 1){
        morseLetter[mlen] = morseWord[i];
        morseLetter[mlen + 1] = '\0';
      }
    }
  }

  if(strlen(morseLetter) > 0){
    morseToText_helper(morseLetter);
  }

  Serial.println("\nConverted Word: ");
  Serial.println(convertedWord);
  morseWord[0] = '\0';
}

void morseToText_helper(char* morseLetter) {//decodes letter by searching in arrays
  for(int i = 0; i < MORSE_TABLE_SIZE; i++){
    if(strcmp(morseLetter, morseTable[i]) == 0){
      int len = strlen(convertedWord);
      if(len < sizeof(convertedWord) - 1){
        convertedWord[len] = charTable[i];
        convertedWord[len + 1] = '\0';
      }
      return;
    }
  }
  strcat(convertedWord, "?");
}

void printLCD(bool currentModeState1){//print convertedWord and based on mode print default text
  lcd.clear();
  lcd.print(convertedWord);
  delay(2000);
  lcd.clear();
  if(currentModeState1 == LOW){
    lcd.print("Enter morse...");
  }else{
    lcd.print("Listening...");
  }
}

void fancyEffects(){//some fancy beeping and lighting based on morseWord
  for(int i = 0; i < strlen(morseWord); i++){
    if(morseWord[i] == '.'){
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
      delay(TIME_UNIT);
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, LOW);
    }else if(morseWord[i] == '-'){
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
      delay(TIME_UNIT * 3);
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, LOW);
    }else if(morseWord[i] == '/'){
      delay(TIME_UNIT * 3);
    }
    delay(TIME_UNIT);
  }
}