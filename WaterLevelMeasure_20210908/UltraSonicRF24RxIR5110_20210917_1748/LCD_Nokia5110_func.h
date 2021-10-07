#define LCD_CMD   LOW
#define LCD_DATA  HIGH
#define LCD_X     84
#define LCD_Y     48
//char itoa_buffer [21];
//    itoa(timeout, itoa_buffer, 10);
//    lcd_print(itoa_buffer);


void LcdWrite(byte dc, byte data) { // dc=data/cmd_
  digitalWrite(PIN_DC, dc);
  digitalWrite(PIN_SCE, LOW);
  shiftOut(PIN_SDIN, PIN_SCLK, MSBFIRST, data);
  digitalWrite(PIN_SCE, HIGH);
}

void LcdCharacter(char character) {
  LcdWrite(LCD_DATA, 0x00);
  for (int index = 0; index < 5; index++)
  {
    LcdWrite(LCD_DATA, ASCII[character - 0x20][index]);
  }
  LcdWrite(LCD_DATA, 0x00);
}

void LcdClear(void) {
  for (int index = 0; index < LCD_X * LCD_Y / 8; index++)
  {
    LcdWrite(LCD_DATA, 0x00);
  }
}

void LcdString(char *characters) {
  while (*characters)
  {
    LcdCharacter(*characters++);
  }
}

void lcd_print(char *characters) {
  while (*characters)
  {
    LcdCharacter(*characters++);
  }
}

// gotoXY routine to position cursor 
// x - range: 0 to 84
// y - range: 0 to 5
void gotoXY(int x, int y) {
  LcdWrite( 0, 0x80 | x);  // Column.
  LcdWrite( 0, 0x40 | y);  // Row.  
}

void drawLine(void) {
unsigned char  j;  
  for(j=0; j<84; j++) { // top
    gotoXY (j,0);
    LcdWrite (1,0x01);
  }   
  for(j=0; j<84; j++) { //Bottom
    gotoXY (j,5);
    LcdWrite (1,0x80);
  }   
  for(j=0; j<6; j++) { // Right
    gotoXY (83,j);
    LcdWrite (1,0xff);
  }   
  for(j=0; j<6; j++) { // Left
    gotoXY (0,j);
    LcdWrite (1,0xff);
  }

}

void lcd_setCursor (int column, int row) { // assume fontWidth fontHeight
  gotoXY(font_Width*(column), (row)); //  font_Height
}

void LcdInitialise(void) {
  pinMode(PIN_SCE,   OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_DC,    OUTPUT);
  pinMode(PIN_SDIN,  OUTPUT);
  pinMode(PIN_SCLK,  OUTPUT);
//  pinMode(PIN_VCC,   OUTPUT);
  pinMode(PIN_BL,    OUTPUT);
//  pinMode(PIN_GND,   OUTPUT);
//  digitalWrite(PIN_VCC, HIGH);     // POWER LCD
  digitalWrite(PIN_BL,  lcd_backlight);     // LCD backlight
  //digitalWrite(PIN_BL,  LOW);      // POWER LCD
//  digitalWrite(PIN_GND, LOW);      // POWER LCD
  digitalWrite(PIN_RESET, LOW);
  //delay(1);
  digitalWrite(PIN_RESET, HIGH);
  LcdWrite( LCD_CMD, 0x21 );  // LCD Extended Commands.
  //LcdWrite( LCD_CMD, 0xBF );  // Set LCD Vop (Contrast). //BF too dark
  //LcdWrite( LCD_CMD, 0xB3 );  // Set LCD Vop (Contrast). //Bn
  LcdWrite( LCD_CMD, 0xB0 );  // Set LCD Vop (Contrast). //B1
  LcdWrite( LCD_CMD, 0x04 );  // Set Temp coefficent. //0x04
  LcdWrite( LCD_CMD, 0x14 );  // LCD bias mode 1:48. //0x13
  LcdWrite( LCD_CMD, 0x0C );  // LCD in normal mode. 0x0d for inverse
  LcdWrite( LCD_CMD, 0x20);
  LcdWrite( LCD_CMD, 0x0C);
}
