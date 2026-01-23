/* ================= MOTOR PINS ================= */
#define ENA 25
#define IN1 26
#define IN2 27

#define ENB 14
#define IN3 16
#define IN4 17

/* ================= IR SENSORS ================= */
#define S1 33
#define S2 32
#define S3 35
#define S4 34
#define S5 39

/* ================= MOTOR PARAMS ================= */
#define MAX_SPEED  200
#define BASE_SPEED 120

/* ================= PID PARAMS ================= */
float Kp = 23.2;
float Ki = 0.03;
float Kd = 15.0;

float error = 0;
float lastError = 0;
float integral = 0;

/* ================= BASE DETECTION ================= */
const unsigned long BASE_STOP_TIME = 5000; // 5 seconds
bool onBase = false;
unsigned long baseStartTime = 0;

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // Sensor pins
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  // PWM setup (ESP32 core 3.x)
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);

  stopMotors();
}

void loop() {
  // Read sensors
  int s1 = digitalRead(S1);
  int s2 = digitalRead(S2);
  int s3 = digitalRead(S3);
  int s4 = digitalRead(S4);
  int s5 = digitalRead(S5);

  // Detect base
  if (!onBase && s1 == LOW && s2 == LOW && s3 == LOW && s4 == LOW && s5 == LOW) {
    onBase = true;
    baseStartTime = millis();
    stopMotors();
    integral = 0;    // reset integral term
    lastError = 0;   // reset derivative term
    Serial.println("BASE DETECTED! Pausing...");
  }

  // If on base, wait until stop time passes
  if (onBase) {
    if (millis() - baseStartTime >= BASE_STOP_TIME) {
      onBase = false;
      Serial.println("Resuming line following...");
    } else {
      return; // skip PID while paused
    }
  }

  // Normal PID line following
  readError();
  runPID();
}

/* ================= PID LOGIC ================= */
void readError() {
  int s1 = digitalRead(S1);
  int s2 = digitalRead(S2);
  int s3 = digitalRead(S3);
  int s4 = digitalRead(S4);
  int s5 = digitalRead(S5);

  error = 0;
  int count = 0;

  if (s1 == LOW) { error += -2; count++; }
  if (s2 == LOW) { error += -1; count++; }
  if (s3 == LOW) { error +=  0; count++; }
  if (s4 == LOW) { error +=  1; count++; }
  if (s5 == LOW) { error +=  2; count++; }

  if (count == 0) {
    error = lastError;   // line lost → continue last correction
  } else {
    error /= count;      // weighted average
  }
}

void runPID() {
  float P = error;

  integral += error;
  integral = constrain(integral, -50, 50); // anti-windup

  float I = integral;
  float D = error - lastError;

  float correction = (Kp * P) + (Ki * I) + (Kd * D);

  int leftSpeed  = BASE_SPEED + correction;
  int rightSpeed = BASE_SPEED - correction;

  leftSpeed  = constrain(leftSpeed,  0, MAX_SPEED);
  rightSpeed = constrain(rightSpeed, 0, MAX_SPEED);

  moveForward(leftSpeed, rightSpeed);
  lastError = error;
}

/* ================= MOTOR CONTROL ================= */
void moveForward(int leftSpeed, int rightSpeed) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);

  ledcWrite(ENA, leftSpeed);
  ledcWrite(ENB, rightSpeed);
}

void stopMotors() {
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);

  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

