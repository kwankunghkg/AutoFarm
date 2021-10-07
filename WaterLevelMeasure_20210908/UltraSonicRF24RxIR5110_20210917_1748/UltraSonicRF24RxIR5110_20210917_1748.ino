/*
 * Water Level Measurement 
 *   ultrasonic HC-SR04 + nRF24 + InfraRed + LCD1602 + Nokia5110
 * Date 20210918 
 */

 #include <EEPROM.h>
#include "define.h"
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
//#ifdef LCD_1602
#include "ascii.h" 
//#endif

#include <IRremote.h>
#include "IRtx_code.h"
int RECV_PIN = A2; // TSOP1838 pinout sphere face (LEFT most) OUTput / GND (middle) / VCC (RIGHT most)
IRrecv irrecv(RECV_PIN);
decode_results results;

// I2C (WIRE/TWI)
// SDA – Arduino Analog Pin 4 (Arduino Mega Pin 20)
// SCL – Arduino Analog Pin 5 (Arduino Mega Pin 21)
//   http://coopermaa2nd.blogspot.tw/2012/09/i2c-16x2-lcd.html

#ifdef LCD_1602
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display NXP PCF8574T
LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display NXP PCF8574AT (MH)
#include "LCD_custom_character.h"
#endif
#ifdef LCD_NOKIA5110
#define PIN_RESET  4 // 12 // 11 // 2 // A11 // LCD RST .... Pin 1
#define PIN_SCE    5 // 11 // 10 // 3 // A10 // LCD CS  .... Pin 3
#define PIN_DC     6 // 10 //  9 // 4 // A9  // LCD Dat/Com. Pin 5
#define PIN_SDIN   7 //  9 //  8 // 5 // A8  // LCD SPIDat . Pin 6
#define PIN_SCLK   8 //  8 //  7 // 6 // A7  // LCD SPIClk . Pin 4
//#define PIN_VCC    7 // LCD Vcc .... Pin 8
#define PIN_BL    A0 //  6 // LCD Vlcd ... Pin 7
//#define PIN_GND    5 // LCD Gnd .... Pin 2   
boolean lcd_backlight=HIGH;
char itoa_buffer [21];
//    itoa(timeout, itoa_buffer, 10);
//    lcd_print(itoa_buffer);
#endif
#ifdef LCD_NOKIA5110
#include "LCD_Nokia5110_ASCII.h"  
#include "LCD_Nokia5110_func.h"  
#endif

int cursor_position =0;
#ifdef LCD_1602
#define lcd1602_cursor_pos_level   12
#endif
#ifdef LCD_NOKIA5110
#define lcd5110_cursor_pos_level   10
#endif
boolean cursor_blink = 0;
int eeAddress = 0;   // location we want the data to be put
int threshold_full  = DEFAULT_TANK_FULL  ; // EEPROM 
int threshold_empty = DEFAULT_TANK_EMPTY ; // EEPROM 
//#define tank_height_default DEFAULT_TANK_HEIGHT
int tank_height     = DEFAULT_TANK_HEIGHT ; // editmode[2]=1100;
//    int editmode[0]=0; // edit variable
//    int editmode[1]=2; // variable cursor position
#define editmodeMax         2
int editmode[editmodeMax]={0, 2}; // [0]=editmode, [1]=cursor_position, [2]=tank_height}; 
boolean editmode_finished=false;
int tank_overflow=0;
int water_level=0;

unsigned long started_waiting_at, overflow_started_waiting_at, screen_started_waiting_at, timeout;
#define distanceValueBufMax       10
int distanceValue1[3]={0,0,DEFAULT_TANK_HEIGHT}; // [0]=BufPointer, [1]=Middle 4 lower bound , [2]= upper bound
int distanceValue2[3]={0,0,DEFAULT_TANK_HEIGHT};
int distanceValue3[3]={0,0,DEFAULT_TANK_HEIGHT};
int distanceValue4[3]={0,0,DEFAULT_TANK_HEIGHT};
int distanceValueRx1=0, distanceValueRx2=0;
int distanceValueRx3=0, distanceValueRx4=0;
int distanceValue1past[distanceValueBufMax]={0,0,0,0,0, 0,0,0,0,0};
int distanceValue2past[distanceValueBufMax]={0,0,0,0,0, 0,0,0,0,0};
int distanceValueFinal[3]={0, 0, 0} ;
int temp=0, tempDiff=0;
int tank_state_current=0;
enum TANKSTATE {EMPTY, NEUTRAL, FULL};

#define RELAY_OFF             HIGH
#define RELAY_ON              LOW
#define RELAY_ON_PIN            2 // 4
#define RELAY_OFF_PIN           3 // 5 
#define FEEDBACK_LED_PIN       A7 

int relay_on(int relay_pin, int led_pin) {
#define sense_feedback_count      3+2  
int feedback_status[sense_feedback_count];
int index;
  feedback_status[0]=analogRead(FEEDBACK_LED_PIN); // sense LED status (HIGH is the voltage level)
#ifdef VERBOSE_DEBUG_FEEDBACK
  Serial.print("FEEDBACK_LED_PIN[start]=");
  Serial.println(feedback_status[0]);
#endif  
  digitalWrite(relay_pin, RELAY_ON );  // turn ON
  delay(100*3);                            // wait
  for (index=1; index<(sense_feedback_count-1); index++) {
    feedback_status[index]=analogRead(FEEDBACK_LED_PIN); // sense LED status (HIGH is the voltage level)
#ifdef VERBOSE_DEBUG_FEEDBACK
    Serial.print("FEEDBACK_LED_PIN[");
    Serial.print(index);
    Serial.print("]=");
    Serial.println(feedback_status[index]);
#endif  
    delay(100*2);                            // wait
  }
  digitalWrite(relay_pin, RELAY_OFF);  // turn OFF
  delay(100*5);                            // wait
  feedback_status[index]=analogRead(FEEDBACK_LED_PIN); // sense LED status (HIGH is the voltage level)
#ifdef VERBOSE_DEBUG_FEEDBACK
  Serial.print("FEEDBACK_LED_PIN[end0]=");
  Serial.println(feedback_status[index]);
#endif  
  for (index=1; index<(sense_feedback_count-1); index++) {
    feedback_status[sense_feedback_count-1]=analogRead(FEEDBACK_LED_PIN); // sense LED status (HIGH is the voltage level)
#ifdef VERBOSE_DEBUG_FEEDBACK
    Serial.print("FEEDBACK_LED_PIN[end");
    Serial.print(index);
    Serial.print("]=");
    Serial.println(feedback_status[sense_feedback_count-1]);
#endif  
    delay(100*5);                            // wait
  }
//      19:22:17.680 -> The function decode(&results)) is deprecated and may not work as expected! Just use decode() without a parameter and IrReceiver.decodedIRData.<fieldname> .
//      19:22:17.680 -> IR.recv=0xFFA857
//      19:22:17.727 -> FEEDBACK_LED_PIN[start]=2
//      19:22:18.050 -> FEEDBACK_LED_PIN[1]=410
//      19:22:18.237 -> FEEDBACK_LED_PIN[2]=410
//      19:22:18.425 -> FEEDBACK_LED_PIN[3]=411
//      19:22:19.129 -> FEEDBACK_LED_PIN[end0]=243
//      19:22:19.129 -> FEEDBACK_LED_PIN[end1]=244
//      19:22:19.641 -> FEEDBACK_LED_PIN[end2]=214
//      19:22:20.157 -> FEEDBACK_LED_PIN[end3]=187
//      19:22:26.037 -> IR.recv=0xFFE01F
//      19:22:26.082 -> FEEDBACK_LED_PIN[start]=1
//      19:22:26.364 -> FEEDBACK_LED_PIN[1]=410
//      19:22:26.594 -> FEEDBACK_LED_PIN[2]=410
//      19:22:26.782 -> FEEDBACK_LED_PIN[3]=409
//      19:22:27.485 -> FEEDBACK_LED_PIN[end0]=244
//      19:22:27.485 -> FEEDBACK_LED_PIN[end1]=244
//      19:22:27.997 -> FEEDBACK_LED_PIN[end2]=215
//      19:22:28.471 -> FEEDBACK_LED_PIN[end3]=187
}

void updateVariable(int data) {
int digit[3]={0, 0, 0};
int variable=0;

  if (editmode[0]>0) {
    if (editmode[0]==1) { variable = threshold_full  ;}
    if (editmode[0]==2) { variable = threshold_empty ;}
    if (editmode[0]==3) { variable = tank_height     ;}
    digit[0] = variable%10 ;
    digit[1] = (variable%100-digit[0])/10 ;
    digit[2] = (variable-digit[0]-(digit[1]*10))/100 ;
    digit[editmode[1]] = data ;
    variable = digit[0]+(digit[1]*10)+(digit[2]*100) ;
    if (editmode[0]==1) { threshold_full  = variable ; }
    if (editmode[0]==2) { threshold_empty = variable ; }
    if (editmode[0]==3) { tank_height     = variable ; }
    }
}

void lcd1602_display_variable(int col, int row, int var, int digits) {
int index, indexA;  
int temp, denom;
  denom = 1;
  for (index=0; index<digits; index++) {
    denom *= 10;
  }
  if ((var<0) || (var>=(denom))) {
#ifdef VERBOSE_DEBUG_VAR    
    Serial.print("lcd1602_display_over "); Serial.print(var);
#endif
#ifdef LCD_1602
    //lcd.setCursor(col+digits-index-1, row);
    lcd.setCursor(col, row);
    lcd.print("OvOv");
#endif
  } 
  else {
    denom = 1;
    for (index=0; index<digits; index++) {
      denom *= 10;
      temp = var/denom;
  #ifdef VERBOSE_DEBUG_VAR    
      Serial.print("var="); Serial.print(var);
      Serial.print(" index="); Serial.print(index);
      Serial.print(" denom="); Serial.print(denom);
      Serial.print(" temp="); Serial.println(temp);
  #endif
      if (temp <= 0) {break;}
    }
  #ifdef LCD_1602
    lcd.setCursor(col+digits-index-1, row);
    lcd.print(var);
    for (indexA=0; indexA<digits-index-1; indexA++) {
      lcd.setCursor(col+indexA, row);
      lcd.print("0");
      delay(100*2);
    }
  #endif
  }
}

void lcd5110_display_variable(int col, int row, int var, int digits) {
int index, indexA;  
int temp, denom;
  denom = 1;
  for (index=0; index<digits; index++) {
    denom *= 10;
  }
  if ((var<0) || (var>=(denom))) {
#ifdef VERBOSE_DEBUG_VAR    
    Serial.print("lcd5110_display_over "); Serial.print(var);
#endif
#ifdef LCD_NOKIA5110
    //lcd_setCursor(col+digits-index-1, row);
    lcd_setCursor(col, row);
    lcd_print("OvOv");
#endif
  } 
  else {
    denom = 1;
    for (index=0; index<digits; index++) {
      denom *= 10;
      temp = var/denom;
  #ifdef VERBOSE_DEBUG_VAR    
      Serial.print("var="); Serial.print(var);
      Serial.print(" index="); Serial.print(index);
      Serial.print(" denom="); Serial.print(denom);
      Serial.print(" temp="); Serial.println(temp);
  #endif
      if (temp <= 0) {break;}
    }
  #ifdef LCD_NOKIA5110
    lcd_setCursor(col+digits-index-1, row);
    //lcd_print(var);
    itoa(var, itoa_buffer, 10);
    lcd_print(itoa_buffer);
    for (indexA=0; indexA<digits-index-1; indexA++) {
      lcd_setCursor(col+indexA, row);
      lcd_print("0");
      delay(100*2);
    }
  #endif
  }
}

#ifdef LCD_1602
void lcd1602_init() {
  lcd.init();                  // initialize the LCD
  lcd.backlight();
  //lcd.noBacklight();
  lcd.setCursor(0, 0); // (col, row)
  lcd.print("/mm ");  // Print a message to the LCD
  lcd.setCursor(0, 1); // (col, row)
  lcd.print("Rx ");
//  lcd.createChar(0 , custchar[0]);   //Creating custom characters in CG-RAM
//  lcd.createChar(1 , custchar[1]);                              
//  lcd.createChar(2 , custchar[2]);
//  lcd.createChar(3 , custchar[3]);
//  lcd.createChar(4 , custchar[4]);
//  lcd.createChar(5 , custchar[5]);
//  lcd.createChar(6 , custchar[6]);
//  lcd.createChar(7 , custchar[7]); //Creating custom characters in CG-RAM  
}
#endif

#ifdef LCD_NOKIA5110
void lcd5110_init() {
  LcdInitialise();
  LcdClear();
  lcd_setCursor(0, 0); // (col, row)
  lcd_print("/mm ");  // Print a message to the LCD
  lcd_setCursor(0, 1); // (col, row)
  lcd_print("Rx ");
}
#endif

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10 (ce, csn)
RF24 radio(9,10);
// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
//const int role_pin = 3;
//
// Topology
//
// Radio pipe addresses for the 2 nodes to communicate.
//pipes = [0xF0F0F0F0E1, 0xF0F0F0F0D2, 0xD1, 0xD0, 0xC1, 0xC0]
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
//const uint64_t pipes[2] = { 0xF0F0F0F0C1LL, 0xF0F0F0F0C0LL }; // 1
const uint64_t pipes[2] = { 0xF0F0F0F0D1LL, 0xF0F0F0F0D0LL }; // 2
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0E0LL }; // 3
//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//
// The various roles supported by this sketch
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role;

//
// Payload
//
#define min_payload_size             0
#define max_payload_size             11 // 32
#define payload_size_increments_by   1
uint8_t payload_length=0;
int     next_payload_size = min_payload_size;
//int     next_payload_size = max_payload_size;
char    receive_payload[max_payload_size+1]; // +1 to allow room for a terminating NULL char
int     index, indexA; // , indexM;
char    ch;
boolean payload_updated = false ;
boolean distanceValue_updated = false ;
//String  distanceString = "123456";
#define LCD_POS_COL_LEVEL     5
#define LCD_POS_ROW_LEVEL     3

void setup()
  {
  Serial.begin(115200);
  irrecv.enableIRIn(); // Start the receiver
   
  eeAddress = 0;   // location we want the data to be put
  EEPROM.get( eeAddress, threshold_full );
#ifdef VERBOSE_DEBUG_DEFAULT
      Serial.print(F("eeFULL="));
      Serial.print(threshold_full);
#endif
  if (threshold_full <=0) { 
    threshold_full  = DEFAULT_TANK_FULL  ;  // EEPROM 
    EEPROM.put( eeAddress, threshold_full );
  }
  eeAddress += sizeof(int); // Move address to the next byte after float 'f'.  
  EEPROM.get( eeAddress, threshold_empty );
#ifdef VERBOSE_DEBUG_DEFAULT
      Serial.print(F("  eeEMPTY="));
      Serial.print(threshold_empty);
#endif
  if (threshold_empty<=0) { 
    threshold_empty = DEFAULT_TANK_EMPTY ;  // EEPROM 
    EEPROM.put( eeAddress, threshold_empty );
  }
#ifdef VERBOSE_DEBUG_DEFAULT
      Serial.print(F("  /  FULL="));
      Serial.print(threshold_full);
      Serial.print(F("  EMPTY="));
      Serial.println(threshold_empty);
#endif

#ifdef LCD_1602
  lcd1602_init();
//  lcd.init();                  // initialize the LCD
//  lcd.backlight();
//  //lcd.noBacklight();
//  lcd.setCursor(0, 0); // (col, row)
//  lcd.print("/mm ");  // Print a message to the LCD
//  lcd.setCursor(0, 1); // (col, row)
//  lcd.print("Rx ");
  lcd.createChar(0 , custchar[0]);   //Creating custom characters in CG-RAM
  lcd.createChar(1 , custchar[1]);                              
  lcd.createChar(2 , custchar[2]);
  lcd.createChar(3 , custchar[3]);
  lcd.createChar(4 , custchar[4]);
  lcd.createChar(5 , custchar[5]);
  lcd.createChar(6 , custchar[6]);
  lcd.createChar(7 , custchar[7]); //Creating custom characters in CG-RAM  
#endif
#ifdef LCD_NOKIA5110
  lcd5110_init();
//  LcdInitialise();
//  LcdClear();
//  lcd_setCursor(0, 0); // (col, row)
//  lcd_print("/mm ");  // Print a message to the LCD
//  lcd_setCursor(0, 1); // (col, row)
//  lcd_print("Rx ");
#endif
  digitalWrite(RELAY_OFF_PIN, RELAY_OFF);  // turn the LED on (HIGH is the voltage level)
  digitalWrite(RELAY_ON_PIN,  RELAY_OFF);  // turn the LED on (HIGH is the voltage level)
  pinMode(RELAY_ON_PIN,  OUTPUT);
  pinMode(RELAY_OFF_PIN, OUTPUT);

  //
  // Role
  //
  // set up the role pin
  //  pinMode(role_pin, INPUT);
  //  digitalWrite(role_pin,HIGH);
  //  delay(20); // Just to get a solid reading on the role pin
  // read the address pin, establish our role
  //if ( digitalRead(role_pin) )
  if ( LOW ) // HIGH => Tx , LOW => Rx
    role = role_ping_out;
  else
    role = role_pong_back;
  //
  // Print preamble
  //
  //Serial.begin(115200);
  printf_begin();
  //Serial.println(F("RF24/examples/pingpair_dyn/"));
  //Serial.print(F("ROLE: "));
  //Serial.println(role_friendly_name[role]);

  //
  // Setup and configure rf radio
  //
  radio.begin();
  // enable dynamic payloads
  radio.enableDynamicPayloads();
  // optionally, increase the delay between retries & # of retries
  radio.setRetries(5,15);

  //
  // Open pipes to other nodes for communication
  //
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  if ( role == role_ping_out ) {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }

  //
  // Start listening
  //
  radio.startListening();

  //
  // Dump the configuration of the rf unit for debugging
  //
  radio.printDetails();

  started_waiting_at = millis(); // initialize
  screen_started_waiting_at = millis(); // initialize
}

void loop()
{
  receive_payload[0] = "D"; // clear to monitor link
  receive_payload[1] = "0"; // clear to monitor link
  receive_payload[2] = "/"; // clear to monitor link
  receive_payload[3] = "0"; // clear to monitor link
  receive_payload[4] = "E"; // clear to monitor link
  payload_length     = 5;
  
  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //
  if ( role == role_pong_back ) {
    // if there is data ready
    while ( radio.available() ) {
      // Fetch the payload, and see if this was the last one.
      payload_length = radio.getDynamicPayloadSize();
      // If a corrupt dynamic payload is received, it will be flushed
      if(!payload_length){
        Serial.print(F("!payload_length continue"));
        continue; 
      }
      radio.read( receive_payload, payload_length );
      // Put a zero at the end for easy printing
      receive_payload[payload_length] = 0;
      // Spew it
#ifdef VERBOSE_DEBUG_RX
      Serial.print(F("Rx="));
      Serial.print(receive_payload);
      Serial.print(F(" / size="));
      Serial.print(payload_length);
#endif
      // First, stop listening so we can talk
      radio.stopListening();
      // Send the final one back.
      radio.write( receive_payload, payload_length );
      //Serial.println(F("Sent response."));
      // Now, resume listening so we catch the next packets.
      radio.startListening();
      started_waiting_at = millis() ;
      payload_updated = true;
    }
  }

  if (payload_updated) {
    for(index=0 ; index<payload_length ; index++) { // search position of END character
      ch=receive_payload[index];
      //Serial.print(F("index="));
      //Serial.println(index);
      //Serial.print(F("  PL[]="));
      //Serial.println(ch);
      if (receive_payload[index]==ASCII_slash) {
        //Serial.print(F("F position = "));
        //Serial.println(index);
        break;
      }
    }
    distanceValueRx1=0;
    for(indexA=1 ; indexA<index ; indexA++) { // convert string to integer
      ch=receive_payload[indexA];
      if (ch>=ASCII_0 and ch<=ASCII_9) {
        distanceValueRx1=distanceValueRx1*10+atoi(&ch);
        //Serial.print(F("indexA="));
        //Serial.print(indexA);
        //Serial.print(F("  VV="));
        //Serial.println(volumeValue);
      }
      else break;
    }
    //indexM=indexA;
    for(index=indexA ; index<=payload_length ; index++) { // search position of END character
      ch=receive_payload[index];
      //Serial.print(F("index="));
      //Serial.println(index);
      //Serial.print(F("  PL[]="));
      //Serial.println(ch);
      if (receive_payload[index]==ASCII_E) {
        //Serial.print(F("F position = "));
        //Serial.println(index);
        break;
      }
    }
    distanceValueRx2=0;
    for(indexA=indexA+1 ; indexA<index ; indexA++) { // convert string to integer
      ch=receive_payload[indexA];
      if (ch>=ASCII_0 and ch<=ASCII_9) {
        distanceValueRx2=distanceValueRx2*10+atoi(&ch);
        //Serial.print(F("indexA="));
        //Serial.print(indexA);
        //Serial.print(F("  dV2="));
        //Serial.println(distanceValue2);
      }
      else break;
    }
  if (++distanceValue1[0] >= distanceValueBufMax) { distanceValue1[0] = 0; }
  if (++distanceValue2[0] >= distanceValueBufMax) { distanceValue2[0] = 0; }
  distanceValue1past[distanceValue1[0]] = distanceValueRx1 ;
  distanceValue2past[distanceValue2[0]] = distanceValueRx2 ;

  for (index=0; index<distanceValueBufMax; index++) {
    distanceValueFinal[1] += distanceValue1past[distanceValue1[0]] ;
    distanceValueFinal[2] += distanceValue2past[distanceValue1[0]] ;
  }
  distanceValueFinal[1] /= (distanceValueBufMax+1) ;
  distanceValueFinal[2] /= (distanceValueBufMax+1) ;

//  temp = abs(distanceValueRx1 - distanceValueFinal[1]) ;
//  tempDiff = (distanceValue1[2] - distanceValue1[1]) ; // /2 ;
//  if ( (distanceValueRx1 < (distanceValueFinal[1]+tempDiff)) && (distanceValueRx1 > (distanceValueFinal[1]-tempDiff)) ) {
//    distanceValue1[1] = (distanceValue1[1] + distanceValueRx1 ) /2 ;
//    distanceValue1[2] = (distanceValue1[2] + distanceValueRx1 ) /2 ;
//    distanceValueFinal[1] = (distanceValue1[1] + distanceValue1[2] ) /2 ;
//  }
//  temp = abs(distanceValueRx2 - distanceValueFinal[2]) ;
//  tempDiff = (distanceValue2[2] - distanceValue2[1]) ; // /2 ;
//  if ( (distanceValueRx2 < (distanceValueFinal[2]+tempDiff)) && (distanceValueRx2 > (distanceValueFinal[2]-tempDiff)) ) {
//    distanceValue2[1] = (distanceValue2[1] + distanceValueRx2 ) /2 ;
//    distanceValue2[2] = (distanceValue2[2] + distanceValueRx2 ) /2 ;
//    distanceValueFinal[2] = (distanceValue2[1] + distanceValue2[2] ) /2 ;
//  }

  distanceValueFinal[0] = (distanceValueFinal[1] + distanceValueFinal[2]) /2 ;
  payload_updated = false;
  distanceValue_updated = true;
  }

  if (distanceValue_updated) {
#ifdef VERBOSE_DEBUG_DISTANCE_1
    Serial.print(" mm1=");
    Serial.print(distanceValueRx1);
    Serial.print(" mm2=");
    Serial.print(distanceValueRx2);
#endif
#ifdef VERBOSE_DEBUG_DISTANCE_2
//  Serial.print(" V1[0]=");
//  Serial.print(distanceValue1[0]);
//  Serial.print(" V1[1]=");
//  Serial.print(distanceValue1[1]);
//  Serial.print(" V1[2]=");
//  Serial.print(distanceValue1[2]);
  Serial.print(" F[1]=");
  Serial.print(distanceValueFinal[1]);
//  Serial.print(" V2[0]=");
//  Serial.print(distanceValue2[0]);
//  Serial.print(" V2[1]=");
//  Serial.print(distanceValue2[1]);
//  Serial.print(" V2[2]=");
//  Serial.print(distanceValue2[2]);
  Serial.print(" F[2]=");
  Serial.print(distanceValueFinal[2]);
  Serial.print(" F[0]=");
  //Serial.println(distanceValueFinal[0]);
  water_level = tank_height-distanceValueFinal[0];
  if ( water_level > 0 ) {
    Serial.println(water_level);
  }
  else { 
    Serial.println("Er-");
  }
#endif  

//
// diaplay water level & tank status
//
#ifdef LCD_1602
    // set the cursor to column 0, line 1
    // (note: line 1 is the second row, since counting begins with 0):
    //lcd.setCursor(3, 1);
    //lcd.print("                ");
    //lcd.setCursor(3, 1);
    //lcd.print(receive_payload);
    //lcd.setCursor(LCD_POS_COL_LEVEL, 0);  
    //lcd.print(millis()/1000); // print the number of seconds since reset:
    //lcd.print("                ");
    //lcd.setCursor(LCD_POS_COL_LEVEL, 0);
    //lcd.print(distanceValue1);
    //lcd.setCursor(LCD_POS_COL_LEVEL+6, 0);
    //lcd.print(distanceValue2);
    lcd.setCursor(0, 1); // (col, row)
    // lcd.print("Rx ");
    // lcd_display_variable(4, 0, distanceValueRx1, 4);
    // lcd_display_variable(4, 1, distanceValueRx2, 4);
#endif
#ifdef LCD_NOKIA5110
    lcd_setCursor(0, 1); // (col, row)
#endif
    if (tank_state_current==FULL)     { 
#ifdef LCD_1602
      lcd.print("Hi "); 
#endif
#ifdef LCD_NOKIA5110
      lcd_print("Hi "); 
#endif
      }
    if (tank_state_current==NEUTRAL)  { 
#ifdef LCD_1602
      lcd.print("OK "); 
#endif
#ifdef LCD_NOKIA5110
      lcd_print("OK "); 
#endif
      }
    if (tank_state_current==EMPTY)    { 
#ifdef LCD_1602
      lcd.print("Lo "); 
#endif
#ifdef LCD_NOKIA5110
      lcd_print("Lo "); 
#endif
      }
#ifdef LCD_1602
    lcd1602_display_variable(3, 1, distanceValueRx1, 4);
    lcd1602_display_variable(7, 1, distanceValueRx2, 4);
#endif
#ifdef LCD_NOKIA5110
    lcd5110_display_variable(1, 5, distanceValueRx1, 4);
    lcd5110_display_variable(7, 5, distanceValueRx2, 4);
#endif
    water_level = tank_height-distanceValueFinal[0];
    if ( water_level >= 0 && water_level<=DEFAULT_TANK_HEIGHT ) {
#ifdef LCD_1602
      lcd1602_display_variable(LCD_POS_COL_LEVEL, 0, water_level, 4);
#endif
#ifdef LCD_NOKIA5110
      lcd5110_display_variable(LCD_POS_COL_LEVEL, LCD_POS_ROW_LEVEL, water_level, 4);
#endif
    }
    else {
#ifdef LCD_1602
      //lcd_display_text(8, 0, "Er-")
      lcd.setCursor(LCD_POS_COL_LEVEL, 0); // (col, row)
      lcd.print("Er");
#endif
#ifdef LCD_NOKIA5110
      //lcd_display_text(8, 0, "Er-")
      lcd_setCursor(LCD_POS_COL_LEVEL, LCD_POS_ROW_LEVEL); // (col, row)
      lcd_print("Er");
#endif
      if (water_level < 0) { 
#ifdef LCD_1602
        lcd.print("- "); 
#endif
#ifdef LCD_NOKIA5110
        lcd_print("- "); 
#endif
      }
      else { 
#ifdef LCD_1602
        lcd.print("+ "); 
#endif
#ifdef LCD_NOKIA5110
        lcd_print("+ "); 
#endif
      }
    }
    distanceValue_updated = false;
  }

  if (  (millis() - screen_started_waiting_at) / 1000 > 20 ) { // timeout screen partial refresh
#ifdef LCD_1602
//    lcd.setCursor(3, 0);
//    lcd.print("             ");
//    lcd.setCursor(3, 1);
//    lcd.print("             ");
  lcd1602_init();
#endif
#ifdef LCD_NOKIA5110
//    lcd_setCursor(3, 0);
//    lcd_print("    ");
//    lcd_setCursor(3, 1);
//    lcd_print("    ");
  lcd5110_init();
#endif
    screen_started_waiting_at = millis() ;
  }

  timeout = (millis() - started_waiting_at) / 1000 ;
  if ( timeout > 5 ) {
    Serial.print(F("       !!! time out !!!  "));
    Serial.println(timeout);
#ifdef LCD_1602
    lcd.setCursor(0, 1);
    lcd.print("TimO");
    //lcd.setCursor(0, 1);
    lcd.print(timeout);
#endif
#ifdef LCD_NOKIA5110
    lcd_setCursor(0, 2);
    lcd_print("TimO");
    lcd_setCursor(0+4, 2);
    itoa(timeout, itoa_buffer, 10);
    lcd_print(itoa_buffer);
#endif
    //started_waiting_at = millis() ;
  }
  delay(100);
  
  //
  // tank state machine
  //
  if (distanceValueFinal[0] > (tank_height - threshold_empty) && (tank_state_current==NEUTRAL)) { 
#ifdef VERBOSE_DEBUG_TANK_STATE    
    Serial.print(F("enter tank empty state"));
    Serial.println(); // timeout
#endif
    tank_state_current = EMPTY;
    relay_on(RELAY_ON_PIN, FEEDBACK_LED_PIN);
    //pump_on_com = ON;
    //pump_off_com = OFF;
    //pump_state = UNDER_DEVEL;
  }
  if ( ((distanceValueFinal[0] < (tank_height - threshold_empty)) && (tank_state_current==EMPTY)) ||
       ((distanceValueFinal[0] > (tank_height - threshold_full )) && (tank_state_current==FULL )) ) { 
#ifdef VERBOSE_DEBUG_TANK_STATE    
    Serial.print(F("enter tank neutral state"));
    Serial.println(); // timeout
#endif
    tank_state_current = NEUTRAL;
    //pump_on_com = ON;
    //pump_off_com = OFF;
    //pump_state = UNDER_DEVEL;
  }
  if (distanceValueFinal[0] < (tank_height - threshold_full) && (tank_state_current==NEUTRAL)) {
#ifdef VERBOSE_DEBUG_TANK_STATE    
    Serial.print(F("enter tank full state"));
    Serial.println(); // timeout
#endif
    tank_state_current = FULL;
    relay_on(RELAY_OFF_PIN, FEEDBACK_LED_PIN);
    //pump_on_com = OFF;
    //pump_off_com = ON;
    //pump_state = UNDER_DEVEL;
  }

  // 
  // relay on off state machine
  //
#ifdef VERBOSE_DEBUG_TANK
  Serial.print(" TankState=");
  Serial.print(tank_state_current);
#endif
   if (irrecv.decode(&results) ) {
    
    if (results.value != 0xffffffff){
      Serial.print(" IR.recv=0x");
      Serial.println(results.value, HEX);
      // set the cursor to column 0, line 1
      // (note: line 1 is the second row, since counting begins with 0):
      //lcd.setCursor(0, 1);
      //lcd.print(" ");
      //if (++cursor_position>1) { cursor_position=0; }
      //lcd.setCursor(cursor_position, 1);
      //lcd.print(results.value, HEX);
      //lcd.print(" ");
      switch (results.value) {
        case IRTX_VOL_MINUS:
          relay_on(RELAY_OFF_PIN, FEEDBACK_LED_PIN);
          break;
        case IRTX_VOL_PLUS:
          relay_on(RELAY_ON_PIN, FEEDBACK_LED_PIN);
          break;
        case IRTX_EQ:
          if ( ++editmode[0]>editmodeMax) { editmode[0]=0; editmode[1]=2; editmode_finished=true; }
          break;
        case IRTX_TRACK_PREV:
          if ( editmode[0]>0) { 
            if ( ++editmode[1]>2) { editmode[1]=0; }
            }
          break;
        case IRTX_TRACK_NEXT:
          if ( editmode[0]>0) { 
            if ( --editmode[1]<0) { editmode[1]=2; }
            }
          break;
        case IRTX_NUM_0:
          updateVariable(0);
          break;
        case IRTX_NUM_1:
          updateVariable(1);
          break;
        case IRTX_NUM_2:
          updateVariable(2);
          break;
        case IRTX_NUM_3:
          updateVariable(3);
          break;
        case IRTX_NUM_4:
          updateVariable(4);
          break;
        case IRTX_NUM_5:
          updateVariable(5);
          break;
        case IRTX_NUM_6:
          updateVariable(6);
          break;
        case IRTX_NUM_7:
          updateVariable(7);
          break;
        case IRTX_NUM_8:
          updateVariable(8);
          break;
        case IRTX_NUM_9:
          updateVariable(9);
          break;
        default:
          //digitalWrite(RELAY_ON_PIN,  RELAY_OFF);    // turn relay off
          //digitalWrite(RELAY_OFF_PIN, RELAY_OFF);    // turn relay off
          break;
      }
    }

    irrecv.resume(); // Receive the next value
  }

  if (editmode_finished) {
    eeAddress = 0;   // location we want the data to be put
    EEPROM.get( eeAddress, temp  );
    if (temp != threshold_full ) { 
      EEPROM.put( eeAddress, threshold_full  ); 
#ifdef VERBOSE_DEBUG_DEFAULT
      Serial.print(F("  //  eeAddrFULL="));
      Serial.print(eeAddress);
      Serial.print(F("  eeFULL="));
      Serial.println(threshold_full);
#endif
      }
    eeAddress += sizeof(int); // Move address to the next byte after  
    EEPROM.get( eeAddress, temp  );
    if (temp != threshold_empty) { 
      EEPROM.put( eeAddress, threshold_empty ); 
#ifdef VERBOSE_DEBUG_DEFAULT
      Serial.print(F("  //  eeAddrEMPTY="));
      Serial.print(eeAddress);
      Serial.print(F("  eeEMPTY="));
      Serial.println(threshold_full);
#endif
      }
    editmode_finished=false;
  }

//
// display threshold level
//
#ifdef LCD_1602
  // set the cursor to column 0, line 0 (upper)
  // (note: line 1 is the second row, since counting begins with 0):
  //lcd.setCursor(0, 0); // (column, row)
  //lcd.print(millis()/1000);  // print the number of seconds since reset:
  lcd1602_display_variable(lcd1602_cursor_pos_level, 0, threshold_full, 3);
  //delay(100*2);
  lcd1602_display_variable(lcd1602_cursor_pos_level, 1, threshold_empty, 3);
  //delay(100*2);
  //lcd.print("   ");
  //delay(100*2);
#endif
#ifdef LCD_NOKIA5110
  lcd5110_display_variable(lcd5110_cursor_pos_level-1, 0, tank_height,     4);
  lcd5110_display_variable(lcd5110_cursor_pos_level,   1, threshold_full,  3);
  lcd5110_display_variable(lcd5110_cursor_pos_level,   2, threshold_empty, 3);
  
#endif
  
  if ( editmode[0]>0) { 
      delay(100*1);
#ifdef LCD_1602
      lcd.setCursor(lcd1602_cursor_pos_level+(2-editmode[1]), editmode[0]-1);
      lcd.write(0);                 //-->>>PRINTING/DISPLAYING CUSTOM CHARACTERS
#endif
#ifdef LCD_NOKIA5110
      //lcd_setCursor(lcd5110_cursor_pos_level+(2-editmode[1]), editmode[0]-1);
      //LcdCharacter('_');            //-->>>PRINTING/DISPLAYING CUSTOM CHARACTERS
#endif
      delay(100*1);
  }
  //delay(100*2);

}
