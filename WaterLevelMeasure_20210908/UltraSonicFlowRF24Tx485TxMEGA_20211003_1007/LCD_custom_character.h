//Making and displaying Custom characters on lcd with Arduino Uno and 16×2 lcd
//https://www.engineersgarage.com/making-custom-characters-on-lcd-using-arduino/
//
//LCD Custom Character Generator
//Support character lcd and create code for Arduino.
//https://maxpromer.github.io/LCD-Character-Creator/

//Custom characters byte arrays
byte customchar[8]={B00000,B01010,B00000,B00000,B00000,B00000,B11111,};
byte a[8]={B00000,B01010,B00100,B00100,B00000,B01110,B10001,};
byte s[8]={B00100,B01010,B10001,B10001,B01010,B00100,};
byte s1[8]={B01110,B01010,B11111,B11011,B11111,B01010,B01110,};
byte s2[8]={B01010,B00100,B00100,B01010,B10001,B00100,B10001,};
byte s3[8]={B00100,B01010,B11111,B01010,B10101,B11011,B10001,};
byte s4[8]={0x1F,0x11,0x11,0x11,0x11,0x11,0x1F,};
byte s5[8]={B11111,B11101,B11011,B11101,B11111,B10000,B10000,B10000,};
//Custom characters byte arrays

byte custchar[8][8] = {
  {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111
  },
  {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111
  },
  {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111
  },
  {
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111
  },
  {
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
  },
  {
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
  },
  {
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
  },
  {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
  },
};
