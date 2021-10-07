/**
 * Example using Dynamic Payloads 
 *
 * This is an example of how to use payloads of a varying (dynamic) size. 
 */

// #include <EEPROM.h>
#include "define.h"
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
//#ifdef LCD_1602
#include "ascii.h" 
//#endif

//
// external interrupt for flow sensor
//
const byte interruptPin1 = 2;
const byte interruptPin2 = 3;
volatile unsigned int intrCount1 = 0;
volatile unsigned int intrCount2 = 0;
int index_time=0;
#define ONE_TENTH         10
#define LCD1602_POS_CNT   6

void ex_isr1() {
  intrCount1++;
}

void ex_isr2() {
  intrCount2++;
}

/*
 * HCSR04Ultrasonic/examples/UltrasonicDemo/UltrasonicDemo.pde
 *
 * SVN Keywords
 * ----------------------------------
 * $Author: cnobile $
 * $Date: 2011-09-17 02:43:12 -0400 (Sat, 17 Sep 2011) $
 * $Revision: 29 $
 * ----------------------------------
 */

#include <Ultrasonic.h>

#define TRIGGER_PIN1  A0  // 
#define ECHO_PIN1     A1  // 
#define TRIGGER_PIN2  A2  // 
#define ECHO_PIN2     A3  // 

#define RS485_TX_PIN  A0  // RS485 Driver Input
#define RS485_DE_PIN  A6  // RS485 Drive Enable
#define RS485_RE_PIN  A7  // RS485 Rx Enable (active low)
#define RS485_RX_PIN  A1  // RS485 Rx Output
#include "RS485_func.h" 

#include <NeoSWSerial.h>
NeoSWSerial NeoSWSerial( RS485_RX_PIN, RS485_TX_PIN);

//Ultrasonic ultrasonic1(TRIGGER_PIN1, ECHO_PIN1);
Ultrasonic ultrasonic2(TRIGGER_PIN2, ECHO_PIN2);

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

unsigned long started_waiting_at;
int distanceValue1=0;
int distanceValue2=0;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10 (ce, csn)
RF24 radio(9,10);
// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 3;
//
// Topology
//
// Radio pipe addresses for the 2 nodes to communicate.
//pipes = [0xF0F0F0F0E1, 0xF0F0F0F0D2, 0xD1, 0xD0, 0xC1, 0xC0]
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
#ifdef RF24_CHANNEL_1
const uint64_t pipes[2] = { 0xF0F0F0F0C1LL, 0xF0F0F0F0C0LL }; // 1
#endif
#ifdef RF24_CHANNEL_2
const uint64_t pipes[2] = { 0xF0F0F0F0D1LL, 0xF0F0F0F0D0LL }; // 2
#endif
#ifdef RF24_CHANNEL_3
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0E0LL }; // 3
#endif
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
#define min_payload_size             1
#define max_payload_size             11 // 32
#define payload_size_increments_by   1
uint8_t payload_length=0;
//int     next_payload_size = min_payload_size;
int     next_payload_size = max_payload_size;
char    receive_payload[max_payload_size+1]; // +1 to allow room for a terminating NULL char
int     index, indexA;
char    ch;

int    distanceStringLength, distanceStringLength1, distanceStringLength2;
String distanceString = "12345678901";
//static char distanceString = "123456";
static char send_payload[] = "D9999/9999E"; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";

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
  // test 
  gotoXY(1, 2); // (col, row)
  lcd_print("/mm ");  // Print a message to the LCD
  gotoXY(1, 3); // (col, row)
  lcd_print("Rx ");
}
#endif

volatile uint32_t newlines = 0UL;
    
static void handleRxChar( uint8_t c )
{
  if (c == '\n')
    newlines++;
}


void setup()
  {
  Serial.begin(115200);
  printf_begin();
  pinMode(interruptPin1, INPUT_PULLUP);
  pinMode(interruptPin2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin1), ex_isr1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(interruptPin2), ex_isr2, CHANGE);
#ifdef LCD_1602
  lcd1602_init();                  // initialize the LCD
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

  pinMode(RS485_TX_PIN, OUTPUT);
  pinMode(RS485_DE_PIN, OUTPUT);
  pinMode(RS485_RE_PIN, OUTPUT);
  pinMode(RS485_RX_PIN, INPUT);
  //digitalWrite(RS485_TX_PIN, HIGH);
  //digitalWrite(RS485_DE_PIN, HIGH);
  //digitalWrite(RS485_RE_PIN, HIGH);
  //digitalWrite(RS485_RX_PIN, HIGH);
  RS485_TXmode();
  NeoSWSerial.attachInterrupt( handleRxChar );
  NeoSWSerial.begin( 9600 );
  NeoSWSerial.write("NeoSWserial Begin\n");
  //
  // Role
  //
  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  //if ( digitalRead(role_pin) )
  if ( HIGH ) // HIGH => Tx , LOW => Rx
    role = role_ping_out;
  else
    role = role_pong_back;
  //
  // Print preamble
  //
  //Serial.begin(115200);
  //printf_begin();
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
  started_waiting_at = millis();
}

void loop()
  {
#ifdef TEST_MODE
  intrCount1++;
#endif
  //distanceString = "123456";
  //send_payload[] = "D9999E"; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";
//float  cmMsec1, inMsec1;
float  cmMsec2, inMsec2;
//long   microsec1 = ultrasonic1.timing();
//  cmMsec1 = ultrasonic1.convert(microsec1, Ultrasonic::CM);
//  inMsec1 = ultrasonic1.convert(microsec1, Ultrasonic::IN);
  if (intrCount1 >9999) {intrCount1 = 0;}
  if (intrCount2 >9999) {intrCount2 = 0;}
  distanceValue1 = intrCount1; // cmMsec1*10 ; 
  if (distanceValue1 >9999) {distanceValue1 = 9999;}
  if (distanceValue1 <0)    {distanceValue1 = 9999;}
  distanceStringLength1 = 1;
  if (distanceValue1 >9)   {distanceStringLength1 = 2;}
  if (distanceValue1 >99)  {distanceStringLength1 = 3;}
  if (distanceValue1 >999) {distanceStringLength1 = 4;}
  delay(200);
long   microsec2 = ultrasonic2.timing();
  cmMsec2 = ultrasonic2.convert(microsec2, Ultrasonic::CM);
  inMsec2 = ultrasonic2.convert(microsec2, Ultrasonic::IN);
  distanceValue2 = cmMsec2*10 ; 
  if (distanceValue2 >9999) {distanceValue2 = 9999;}
  if (distanceValue2 <0)    {distanceValue2 = 9999;}
  distanceStringLength2 = 1;
  if (distanceValue2 >9)   {distanceStringLength2 = 2;}
  if (distanceValue2 >99)  {distanceStringLength2 = 3;}
  if (distanceValue2 >999) {distanceStringLength2 = 4;}
  distanceString = "D" + String(distanceValue1) + "/" + String(distanceValue2) + "E";// using a float and the decimal places
  distanceStringLength = distanceStringLength1 + distanceStringLength2 ;
  next_payload_size = distanceStringLength+2+1;
  for(index=0; index<=next_payload_size; index++) {
    send_payload[index] = distanceString[index];
  }
  //send_payload[index] = "";
  //send_payload = distanceString;
  //Serial.print("MS:");
  //Serial.print(microsec);
  //Serial.print(" CM:");
  //Serial.print(cmMsec);
  //Serial.print(" IN:");
  //Serial.print(inMsec);
  Serial.print(" mm1=");
  Serial.print(distanceValue1);
  //Serial.print("=");
  //Serial.print(distanceString1);
  Serial.print(" mm2=");
  Serial.print(distanceValue2);
  Serial.print(" _");
  Serial.print(distanceString);
  //NeoSWSerial.write("NSS..."); // VERBOSE DEBUG RS485
  //NeoSWSerial.write(distanceString);
  NeoSWSerial.write(send_payload);
  //NeoSWSerial.write(distanceValue2);
  //NeoSWSerial.write("...\n\r"); // VERBOSE DEBUG RS485
  //Serial.print(" Tx=");
  //Serial.print(send_payload);
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
#ifdef LCD_1602
  lcd.setCursor(LCD1602_POS_1, 0);  
  // print the number of seconds since reset:
  //lcd.print(millis()/1000);
  lcd.print("                ");
  lcd.setCursor(LCD1602_POS_1, 0);
  lcd.print(distanceValue1);
  lcd.setCursor(LCD1602_POS_1+6, 0);
  lcd.print(distanceValue2);
#endif
#ifdef LCD_NOKIA5110
  lcd_setCursor(LCD5110_POS_1, 0);  
  lcd_print("         ");
  lcd_setCursor(LCD5110_POS_1, 0);
  //itoa(distanceValue1, itoa_buffer, 10);
  itoa(distanceValue2, itoa_buffer, 10);
  lcd_print(itoa_buffer);
//  lcd_setCursor(LCD5110_POS_1+5, 0);
//  itoa(distanceValue2, itoa_buffer, 10);
//  lcd_print(itoa_buffer);
  lcd_setCursor(LCD5110_POS_1+5, 4);
  itoa(intrCount1, itoa_buffer, 10);
  lcd_print(itoa_buffer);
  lcd_setCursor(LCD5110_POS_1+5, 5);
  itoa(intrCount2, itoa_buffer, 10);
  lcd_print(itoa_buffer);
#endif
  //delay(100);

//*  
  //
  // Ping out role.  Repeatedly send the current time
  //
  if (role == role_ping_out)
  {
    // The payload will always be the same, what will change is how much of it we send.
    //static char send_payload[] = "D9999E"; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";
    //static char send_payload[] = distanceString; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";
    // First, stop listening so we can talk.
    radio.stopListening();
    radio.write( send_payload, next_payload_size );
    // Now, continue listening
    radio.startListening();
    // Take the time, and send it.  This will block until complete
    Serial.print(F(" Tx="));
    Serial.print(send_payload);
    Serial.print(F(" / size="));
    Serial.print(next_payload_size);
    delay(200);
    // Wait here until we get a response, or timeout
    if ( radio.available() ) {
      // Grab the response, compare, and send to debugging spew
      payload_length = radio.getDynamicPayloadSize();
      // If a corrupt dynamic payload is received, it will be flushed
      //if(!payload_length) { return; }
      radio.read( receive_payload, payload_length );
      // Put a zero at the end for easy printing
      receive_payload[payload_length] = 0;
      // Spew it
      Serial.print(F(" Rx="));
      Serial.print(receive_payload);
      Serial.print(F(" / size="));
      Serial.print(payload_length);
#ifdef LCD_1602
      lcd.setCursor(3, 1);
      lcd.print("                ");
      lcd.setCursor(3, 1);
      lcd.print(receive_payload);
#endif
#ifdef LCD_NOKIA5110
      lcd_setCursor(3, 1);
      lcd_print("                ");
      lcd_setCursor(3, 1);
      lcd_print(receive_payload);
#endif
      started_waiting_at = millis() ;
    }
  }

  if (millis() - started_waiting_at > 5000 ) {
    Serial.println(F("                                  !!! timed out !!!"));
    started_waiting_at = millis() ;
  }
  Serial.println(F(""));
  delay(200);

}
