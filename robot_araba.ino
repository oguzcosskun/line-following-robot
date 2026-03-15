#include <IRremote.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === IR Kumanda Kodları ===
const int irPin = 2;
const int forwardCode = 24;
const int backwardCode = 82;
const int leftCode = 8;
const int rightCode = 90;
const int stopCode = 28;
const int followLineCode = 69;
const int obstacleAvoidanceCode = 70;

// === Motor Sürücü ===
const int enA = 6;
const int in1 = 7;
const int in2 = 3;
const int enB = 5;
const int in3 = 4;
const int in4 = 8;

// === Çizgi Sensörleri ===
const int sensorLeft = A0;
const int sensorMid = A1;
const int sensorRight = A2;
const int threshold = 550;

// === LDR + LED ===
const int ldrPin = 10;
const int ledPin = A3;

// === Ultrasonik + Servo ===
const int trigPin = 12;
const int echoPin = 13;
Servo myServo;
const int servoPin = 9;

// === LCD Ekran (Yeni adres 0x27) ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === Mod Kontrolleri ===
bool followLineMode = false;
bool obstacleAvoidanceMode = false;

// === Süre Takibi ===
unsigned long followStartTime = 0;

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);

  pinMode(enA, OUTPUT); pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(enB, OUTPUT); pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  pinMode(ldrPin, INPUT); pinMode(ledPin, OUTPUT);
  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);

  myServo.attach(servoPin);
  myServo.write(90);

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("  Sistem Hazir");
  delay(1000);
  lcd.clear();
}

void loop() {
  // === LDR kontrolü (karanlıkta LED yanar) ===
  int light = digitalRead(ldrPin);
  digitalWrite(ledPin, light == HIGH ? HIGH : LOW);

  // === Kumanda sinyali ===
  if (IrReceiver.decode()) {
    int code = IrReceiver.decodedIRData.command;
    Serial.print("Kod: "); Serial.println(code);

    if (code == followLineCode) {
      followLineMode = true;
      obstacleAvoidanceMode = false;
      followStartTime = millis();
      lcd.clear();
    } else if (code == obstacleAvoidanceCode) {
      obstacleAvoidanceMode = true;
      followLineMode = false;
      lcd.clear();
    } else if (code == stopCode) {
      followLineMode = false;
      obstacleAvoidanceMode = false;
      Stop();
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("MOD: DURDU");
    } else if (!followLineMode && !obstacleAvoidanceMode) {
      if (code == forwardCode) Forward();
      else if (code == backwardCode) Backward();
      else if (code == leftCode) Right();  // yön ters çünkü sağ-sol algısı sensörlere göre
      else if (code == rightCode) Left();
      else Stop();
    }

    IrReceiver.resume();
  }

  if (followLineMode) runLineFollower();
  if (obstacleAvoidanceMode) runObstacleAvoidance();
}

// === Hareket Fonksiyonları ===
void Forward() {
  analogWrite(enA, 200); analogWrite(enB, 200);
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
}

void Backward() {
  analogWrite(enA, 200); analogWrite(enB, 200);
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
}

void Left() {
  analogWrite(enA, 200); analogWrite(enB, 200);
  digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
}

void Right() {
  analogWrite(enA, 200); analogWrite(enB, 200);
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, LOW);
}

void Stop() {
  digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, LOW);
}

// === Çizgi Takip Modu ===
void runLineFollower() {
  if (IrReceiver.decode()) {
    int code = IrReceiver.decodedIRData.command;
    if (code == stopCode) {
      followLineMode = false;
      Stop();
      IrReceiver.resume();
      return;
    }
    IrReceiver.resume();
  }

  unsigned long currentTime = millis();
  int seconds = (currentTime - followStartTime) / 1000;
  lcd.setCursor(0, 0); lcd.print("MOD: CIZGI      ");
  lcd.setCursor(0, 1); lcd.print("Sure: "); lcd.print(seconds); lcd.print(" sn ");

  int left = analogRead(sensorLeft);
  int mid = analogRead(sensorMid);
  int right = analogRead(sensorRight);

  bool sL = left > threshold;
  bool sM = mid > threshold;
  bool sR = right > threshold;

  if (!sL && !sM && !sR) {
    Left(); delay(100);
    sL = analogRead(sensorLeft) > threshold;
    sR = analogRead(sensorRight) > threshold;
    if (!sL && !sR) {
      Right(); delay(100);
      if (!sL && !sR) {
        Forward(); delay(300);
        Stop(); delay(100);
        Left(); delay(100);
      }
    }
  } else if (sL && !sM && !sR) {
    Left();
  } else if (!sL && !sM && sR) {
    Right();
  } else if ((sL && sM) || (sM && sR) || (sM && !sL && !sR)) {
    Forward();
  } else if (sL && sM && sR) {
    Forward();
  } else {
    Stop();
  }
}

// === Engel Algılama Modu ===
void runObstacleAvoidance() {
  int distance = measureDistance();
  lcd.setCursor(0, 0); lcd.print("MOD: ENGEL      ");
  lcd.setCursor(0, 1); lcd.print("Mesafe: "); lcd.print(distance); lcd.print(" cm ");

  if (distance < 20) {
    Stop(); delay(200);
    myServo.write(30); delay(300);
    int rightDist = measureDistance();
    myServo.write(150); delay(300);
    int leftDist = measureDistance();
    myServo.write(90);

    if (leftDist > rightDist) {
      Left(); delay(500);
    } else {
      Right(); delay(500);
    }
    Forward(); delay(300);
  } else {
    Forward();
  }
}

int measureDistance() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 20000);
  return duration * 0.034 / 2;
}
