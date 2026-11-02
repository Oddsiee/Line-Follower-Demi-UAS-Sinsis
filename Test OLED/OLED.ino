#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  u8g2.begin();
}

void loop() {
  static unsigned long counter = 0;

  u8g2.clearBuffer();

  // Teksi
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 14, "Faiz Anjing!");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 28, "SH1106 + Arduino Nano");

  // Bentuk dasar
  u8g2.drawFrame(0, 34, 128, 30);
  u8g2.drawCircle(20, 49, 10);
  u8g2.drawBox(40, 40, 20, 18);

  // Counter
  u8g2.setCursor(75, 54);
  u8g2.print(counter++);

  u8g2.sendBuffer();
  delay(1000);
}