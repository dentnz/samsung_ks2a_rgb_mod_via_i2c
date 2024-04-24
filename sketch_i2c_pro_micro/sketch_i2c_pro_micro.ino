#include <I2C.h>

// I2C messaging for the Micronas VDP313X as found in the Samsung KS2a CRT chasis
// Code by dentnz, 2024

//(STAND-BY) (DISPLAY/info) (MENU) (MUTE) (POWER ON)
//(STAND-BY) (INFO) (MENU) (MUTE) (POWER ON)
//(power on)(mute)(menu)(info)(stand-by)

// Need to go into the service menu and set HPOS to -16 in deflection menu
void rgbEnable() {
  I2c._start();
  // slave address - 8a
  I2c._sendAddress(138);
  // RGB enable
  I2c._sendByte(49);
  //I2c._sendByte(5);
  I2c._sendByte(13);
  // OSD priority
  I2c._sendByte(75);
  I2c._sendByte(1);
  I2c._sendByte(0);
  I2c._stop();
}

// Write 16 bits to fastprocessor reg at address 8e
void fpWrite16(int regHigh, int regLow, int datHigh, int datLow) {
  I2c._start();
  I2c._sendAddress(0x8e);
  // FPWR
  I2c._sendByte(0x37);
  I2c._sendByte(regHigh);
  I2c._sendByte(regLow);
  I2c._stop();

  I2c._start();
  I2c._sendAddress(0x8e);
  // FPDAT
  I2c._sendByte(0x38);
  I2c._sendByte(datHigh);
  I2c._sendByte(datLow);
  I2c._stop();
}

// Write 16 bits to reg on 8a, the video backend
void beWrite16(int reg, int datHigh, int datLow) {
  I2c._start();
  I2c._sendAddress(0x8a);
  I2c._sendByte(reg);
  I2c._sendByte(datHigh);
  I2c._sendByte(datLow);
  I2c._stop();
}

// Write 8 bits to reg on 8a
void beWrite8(int reg, int dat) {
  I2c._start();
  I2c._sendAddress(0x8a);
  I2c._sendByte(reg);
  I2c._sendByte(dat);
  I2c._stop();
}

void setup() {
  pinMode(PIN_A0, OUTPUT);
  digitalWrite(PIN_A0, HIGH);
  delay(2000);
  Serial.begin(9600);
  Serial.println(F("Initialize I2C master library for low-level I2C"));
  I2c.begin();
  I2c.setSpeed(false);
  I2c.pullup(true);
  I2c.timeOut(100);
  delay(15000);
  // Aux pin to disable the MICOM
  Serial.println(F("Turning off the MICOM"));
  digitalWrite(PIN_A0, LOW);
  // We need to wait here, else we 
  delay(2000);
  
  Serial.println(F("RGB enabling"));
  rgbEnable();

  delay(2000);
  Serial.println(F("Turning the MICOM back on"));
  digitalWrite(PIN_A0, HIGH);
  beWrite8(0x25, 0x05);

  I2c.end();
}

void loop() {
  delay(1000);
}

/*
  // Disable Automatic Standard Recognition (ASR)
  fpWrite16(0x01,0x48, 0x00,0x00);
  
  // Select VIN4
  //fpWrite16(0x00,0x21, 0x0e,0x03);
  // Set standard to NTSC
  //fpWrite16(0x00,0x20, 0x0e,0x07);
  
  // Re-enable ASR
  //fpWrite(0x01,0x48, 0x08,0x08);
*/