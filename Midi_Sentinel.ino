#include <LiquidCrystal_I2C.h>
#include <MIDIUSB.h>
#include <ResponsiveAnalogRead.h>
#include <CD74HC4067.h>
#include <KY040rotary.h>
#include <Bounce2.h>

#define N_POTS 16  //total number of pots across all muxes
//* Define s0, s1, s2, s3, and x pins
#define s0 11
#define s1 10
#define s2 9
#define s3 8
#define x1 A0  // analog pin of the first mux

#define CLK 4  //Rotary encoder
#define DT 6
#define SW 1

//#define thumbSwitch 7  //thumbstick click
#define jStickX A3
#define jStickY A4
#define thumbX A2
#define thumbY A1
#define thumbSwitch 7

byte stickCtrlX = 17;
byte stickCtrlY = 18;
byte thumbCtrlX = 19;   //Control Number
byte thumbCtrlY = 20;   //"            "
byte midiChannel = 1;   // Set your desired MIDI channel
byte midiPChannel = 0;  // Keep track for display refresh reasons

volatile bool btnPressed = false;   //rotary encoder btn
volatile bool btnPressed2 = false;  //thumbstick btn
bool btnPVal = false;
bool btnPVal2 = false;

float snapMultiplier = 0.10;  // (0.0 - 1.0) - Increase for faster, but less smooth reading

int stickState[4] = { 0 };
int stickPState[4] = { 0 };
int potState[N_POTS] = { 0 };   // Current state of the pot
int potPState[N_POTS] = { 0 };  // Previous state of the pot
bool thumbChanged = false;      // thumbstick has moved

/////////////////////////////////////////////
//Obj Inits
CD74HC4067 mux = CD74HC4067(s0, s1, s2, s3);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Bounce myClick = Bounce();   //debounce for rotary encoder click
Bounce myClick2 = Bounce();  //debounce for thumbstick click

KY040 myEncoder(CLK, DT, SW);
const byte N_POTS_PER_MUX = 16;                                                                     // Number of pots connected to the multiplexer
const byte POT_MUX_PIN[N_POTS_PER_MUX] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };  // Channels on the multiplexer

ResponsiveAnalogRead responsivePots[N_POTS_PER_MUX] = {
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true),
  ResponsiveAnalogRead(x1, true)
};
ResponsiveAnalogRead responsiveThumb[2] = {
  ResponsiveAnalogRead(thumbX, true),
  ResponsiveAnalogRead(thumbY, true)
};
ResponsiveAnalogRead responsiveStick[2] = {
  ResponsiveAnalogRead(jStickX, true),
  ResponsiveAnalogRead(jStickY, true)
};

void potentiometers() {
  for (int i = 0; i < N_POTS_PER_MUX; i++) {
    mux.channel(POT_MUX_PIN[i]);
    delayMicroseconds(8);  // Short delay to allow the analog signal to settle
    int rawReading = analogRead(x1);
    responsivePots[i].update(rawReading);

    int newState = map(responsivePots[i].getValue(), 0, 1023, 0, 127);
    if (potPState[i] != newState) {
      potState[i] = newState;
      Serial.println(potState[i]);
      controlChange(midiChannel - 1, i, potState[i]);
      MidiUSB.flush();
      potPState[i] = potState[i]; // Update the previous state after sending MIDI message
    }
  }
}

void MidiDown() {
  if (btnPressed == true) {

    if (midiChannel == 1) {
      midiChannel = 16;
    } else {
      midiChannel--;
    }
    //Serial.println("Midi Channel Changed Down");
  }
}

void MidiUp() {
  if (btnPressed == true) {
    if (midiChannel == 16) {
      midiChannel = 1;
    } else {
      midiChannel++;
    }
    //Serial.println("Midi Channel Changed Up");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(x1, INPUT);
  pinMode(thumbX, INPUT);
  pinMode(thumbY, INPUT);
  pinMode(jStickX, INPUT);
  pinMode(jStickY, INPUT);
  pinMode(thumbSwitch, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);

  myClick.attach(SW);
  myClick.interval(50);  // interval in ms
  myClick.update();

  myClick2.attach(thumbSwitch);
  myClick2.interval(50);
  myClick2.update();

  myEncoder.OnButtonLeft(MidiDown);
  myEncoder.OnButtonRight(MidiUp);

  for (int i = 0; i < N_POTS_PER_MUX; i++) {
    responsivePots[i].enableEdgeSnap();
    responsivePots[i].setSnapMultiplier(snapMultiplier);
  }

  for (int i = 0; i < 2; i++) {
    responsiveThumb[i].enableEdgeSnap();
    responsiveThumb[i].setSnapMultiplier(snapMultiplier);
  }

  for (int i = 0; i < 2; i++) {
    responsiveStick[i].enableEdgeSnap();
    responsiveStick[i].setSnapMultiplier(snapMultiplier);
  }

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 1);
  lcd.print("MIDI Sentinel Online");
  delay(4000);
  lcd.clear();
  lcdPrint();
}

void interruptFunction() {

  if (btnPressed == true) {
    btnPVal = true;
    btnPressed = false;
  } else {
    btnPVal = false;
    btnPressed = true;
  }
  //Serial.println(btnPressed);
  return;
}

void interruptFunction2() {

  if (btnPressed2 == true) {
    btnPVal2 = true;
    btnPressed2 = false;
  } else {
    btnPVal2 = false;
    btnPressed2 = true;
  }
  return;
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = { 0x0B, 0xB0 | channel, control, value };
  //Serial.println(String(channel) + " " + String(control) + " " + String(value));
  MidiUSB.sendMIDI(event);
}

void Sticks() {
  // Define a threshold for movement detection
  const int movementThreshold = 5; // Adjust this value as needed

  // Thumbstick handling
  if (btnPressed2 == true) {
    //get current thumbstick position and hold
  } else if (btnPressed2 == false) {
    // Thumbstick live tracking
    int rawReading1 = analogRead(thumbX);
    int rawReading2 = analogRead(thumbY);

    responsiveThumb[0].update(rawReading1);
    responsiveThumb[1].update(rawReading2);

    int newStateX = map(responsiveThumb[0].getValue(), 0, 1023, 0, 127);
    int newStateY = map(responsiveThumb[1].getValue(), 0, 1023, 0, 127);

    if (abs(newStateX - stickPState[0]) > movementThreshold) {
      controlChange(midiChannel - 1, thumbCtrlX, newStateX);
      MidiUSB.flush();
      stickPState[0] = newStateX;
    }
    if (abs(newStateY - stickPState[1]) > movementThreshold) {
      controlChange(midiChannel - 1, thumbCtrlY, newStateY);
      MidiUSB.flush();
      stickPState[1] = newStateY;
    }
  }

  // Joystick handling
  int rawReading3 = analogRead(jStickX);
  int rawReading4 = analogRead(jStickY);

  responsiveStick[0].update(rawReading3);
  responsiveStick[1].update(rawReading4);

  int newStateX = map(responsiveStick[0].getValue(), 0, 1023, 0, 127);
  int newStateY = map(responsiveStick[1].getValue(), 0, 1023, 0, 127);

  if (abs(newStateX - stickState[2]) > movementThreshold) {
    controlChange(midiChannel - 1, stickCtrlX, newStateX);
    MidiUSB.flush();
    stickState[2] = newStateX;
  }
  if (abs(newStateY - stickState[3]) > movementThreshold) {
    controlChange(midiChannel - 1, stickCtrlY, newStateY);
    MidiUSB.flush();
    stickState[3] = newStateY;
  }
}

void loop() {
  myEncoder.Process(millis());

  myClick2.update();
  if (myClick2.fell()) {
    interruptFunction2();
  }

  myClick.update();
  if (myClick.fell()) {
    interruptFunction();
  }

  potentiometers();
  Sticks();

  if (btnPressed == true) {
    lcdPrint();
  }
}

void lcdPrint() {

  if (btnPressed == true) {
    lcd.setCursor(7, 2);
    lcd.print("-=");
    lcd.setCursor(11, 2);
    lcd.print("=-");
  }
  lcd.setCursor(4, 0);
  lcd.print("Midi Channel");
  lcd.setCursor(7, 2);

  //add code this line to determine single or double digit
  if (midiChannel < 10) {
    lcd.print("  0" + String(midiChannel) + "  ");
  } else {
    lcd.print("  " + String(midiChannel) + "  ");
  }
  //Serial.println(midiChannel);
  midiPChannel = midiChannel;
  //}
}
