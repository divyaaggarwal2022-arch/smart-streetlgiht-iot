// ============================================================
//  Smart Street Light — 2 IR + 2 PIR + 4 LED + BT (FINAL)
// ============================================================

#define ZS_IDLE   0
#define ZS_BRIGHT 1
#define ZS_FADING 2

// Detection states (Change if sensors trigger inverted)
#define IR_DETECT_STATE  LOW  
#define PIR_DETECT_STATE HIGH

// --- Bluetooth Mode Variable ---
char currentMode = 'A'; // 'A'=Auto, 'B'=All Bright, 'O'=All Off

// --- Pins ---
const int LDR_PIN        = A0;
const int SENSOR_PINS[4] = {4, 5, 2, 3};  
const int LED_PINS[4]    = {6, 9, 10, 11};
const bool IS_PIR[4]     = {false, false, true, true};

// --- Values ---
const int DAY_THRESHOLD  = 800;
const int NIGHT_THRESHOLD = 700;
const int DIM_VAL        = 40;
const int BRIGHT_VAL     = 255;

const unsigned long HOLD_MS = 4000UL;
const int FADE_STEPS        = 200;
const float STEP_SIZE       = (float)(BRIGHT_VAL - DIM_VAL) / (float)FADE_STEPS;

struct ZoneData {
  int state;
  unsigned long motionLastSeen;
  int fadeCtr;
  int pwm;
};

ZoneData zone[4];
bool nightMode = false;

void setup() {
  Serial.begin(9600); // Standard for HC-05
  for (int i = 0; i < 4; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
    pinMode(LED_PINS[i], OUTPUT);
    analogWrite(LED_PINS[i], 0);
    zone[i].state = ZS_IDLE;
  }
}

void setLED(int i, int val) {
  val = constrain(val, 0, 255);
  zone[i].pwm = val;
  analogWrite(LED_PINS[i], val);
}

bool isDetecting(int i) {
  int raw = digitalRead(SENSOR_PINS[i]);
  return (IS_PIR[i]) ? (raw == PIR_DETECT_STATE) : (raw == IR_DETECT_STATE);
}

void loop() {
  unsigned long now = millis();

  // 1. Listen for Bluetooth Commands
  if (Serial.available() > 0) {
    char incoming = Serial.read();
    // Only update if it's a valid command from our App
    if (incoming == 'A' || incoming == 'B' || incoming == 'O') {
      currentMode = incoming;
    }
  }

  // 2. Execute Modes
  if (currentMode == 'B') { // ALL BRIGHT
    for (int i = 0; i < 4; i++) setLED(i, 255);
    return; 
  } 
  
  if (currentMode == 'O') { // ALL OFF
    for (int i = 0; i < 4; i++) setLED(i, 0);
    return;
  }

  // 3. AUTO MODE
  if (currentMode == 'A') {
    int ldr = analogRead(LDR_PIN);
    // LDR Logic
    if (nightMode && ldr > DAY_THRESHOLD) nightMode = false;
    else if (!nightMode && ldr < NIGHT_THRESHOLD) nightMode = true;

    if (!nightMode) {
      for (int i = 0; i < 4; i++) { setLED(i, 0); zone[i].state = ZS_IDLE; }
      return;
    }

    // Motion Logic per zone
    for (int i = 0; i < 4; i++) {
      bool det = isDetecting(i);
      if (zone[i].state == ZS_IDLE) {
        setLED(i, DIM_VAL);
        if (det) { 
          zone[i].state = ZS_BRIGHT; 
          zone[i].motionLastSeen = now; 
          setLED(i, BRIGHT_VAL); 
        }
      } else if (zone[i].state == ZS_BRIGHT) {
        if (det) zone[i].motionLastSeen = now;
        else if (now - zone[i].motionLastSeen >= HOLD_MS) { 
          zone[i].state = ZS_FADING; 
          zone[i].fadeCtr = FADE_STEPS; 
        }
      } else { // FADING
        if (det) { 
          zone[i].state = ZS_BRIGHT; 
          zone[i].motionLastSeen = now; 
          zone[i].fadeCtr = 0; 
          setLED(i, BRIGHT_VAL); 
        } else if (zone[i].fadeCtr > 0) {
          int val = DIM_VAL + (int)(STEP_SIZE * zone[i].fadeCtr);
          setLED(i, val);
          zone[i].fadeCtr--;
        } else { 
          zone[i].state = ZS_IDLE; 
          setLED(i, DIM_VAL); 
        }
      }
    }
  }
}