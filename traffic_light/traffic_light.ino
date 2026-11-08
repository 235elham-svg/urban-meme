/*
 * إشارة مرور ذكية - ESP32
 * حساس ألتراسونك HC-SR04 + 3 لدات (أحمر/أصفر/أخضر) + زر
 *
 * طريقة العمل:
 *  - الإشارة حمراء بشكل افتراضي.
 *  - لما تقرب السيارة (أقل من CAR_DISTANCE_CM) وتضغط الزر:
 *      أحمر -> أصفر (ثانيتين) -> أخضر
 *  - تبقى خضراء لين تبتعد السيارة (أو تنتهي المدة القصوى)، بعدها:
 *      أخضر -> أصفر (ثانيتين) -> أحمر
 *  - إذا ضغطت الزر وما فيه سيارة قريبة، ما يتغير شي.
 */

// ===== الأطراف (ESP32) =====
const int TRIG_PIN   = 5;
const int ECHO_PIN   = 18;   // عبر مقسم جهد (انظر ملف الشرح)
const int RED_PIN    = 25;
const int YELLOW_PIN = 26;
const int GREEN_PIN  = 27;
const int BUTTON_PIN = 4;    // الطرف الثاني للزر على GND

// ===== الإعدادات =====
const float         CAR_DISTANCE_CM = 30.0;  // مسافة اعتبار السيارة "قريبة"
const unsigned long YELLOW_TIME     = 2000;  // مدة الأصفر (ms)
const unsigned long CAR_GONE_TIME   = 3000;  // كم لازم تغيب السيارة قبل الرجوع للأحمر
const unsigned long MAX_GREEN_TIME  = 15000; // أقصى مدة للأخضر
const unsigned long SENSOR_INTERVAL = 100;   // كل كم نقرأ الحساس
const unsigned long DEBOUNCE_TIME   = 50;

enum State { RED, YELLOW_TO_GREEN, GREEN, YELLOW_TO_RED };
State state = RED;
unsigned long stateStart = 0;

float distanceCm = -1;
bool carNear = false;
unsigned long lastSensorRead = 0;
unsigned long lastCarSeen = 0;

int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounce = 0;

void setLights(bool r, bool y, bool g) {
  digitalWrite(RED_PIN, r);
  digitalWrite(YELLOW_PIN, y);
  digitalWrite(GREEN_PIN, g);
}

void changeState(State s) {
  state = s;
  stateStart = millis();
  switch (s) {
    case RED:             setLights(1, 0, 0); Serial.println(">> احمر");  break;
    case YELLOW_TO_GREEN:
    case YELLOW_TO_RED:   setLights(0, 1, 0); Serial.println(">> اصفر");  break;
    case GREEN:           setLights(0, 0, 1); Serial.println(">> اخضر");  break;
  }
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); // مهلة ~5 متر
  if (duration == 0) return -1;                            // ما فيه قراءة
  return duration * 0.0343 / 2.0;
}

// يرجع true مرة وحدة عند الضغط
bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) lastDebounce = millis();
  lastButtonReading = reading;
  if (millis() - lastDebounce > DEBOUNCE_TIME && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  changeState(RED);
}

void loop() {
  unsigned long now = millis();

  // قراءة الحساس
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    distanceCm = readDistanceCm();
    carNear = (distanceCm > 0 && distanceCm < CAR_DISTANCE_CM);
    if (carNear) lastCarSeen = now;
  }

  bool pressed = buttonPressed();

  switch (state) {
    case RED:
      if (pressed) {
        if (carNear) {
          Serial.printf("سيارة على بعد %.1f سم - تحويل للأخضر\n", distanceCm);
          changeState(YELLOW_TO_GREEN);
        } else {
          Serial.println("الزر انضغط بس ما فيه سيارة قريبة");
        }
      }
      break;

    case YELLOW_TO_GREEN:
      if (now - stateStart >= YELLOW_TIME) changeState(GREEN);
      break;

    case GREEN:
      if (now - lastCarSeen >= CAR_GONE_TIME || now - stateStart >= MAX_GREEN_TIME) {
        changeState(YELLOW_TO_RED);
      }
      break;

    case YELLOW_TO_RED:
      if (now - stateStart >= YELLOW_TIME) changeState(RED);
      break;
  }
}
