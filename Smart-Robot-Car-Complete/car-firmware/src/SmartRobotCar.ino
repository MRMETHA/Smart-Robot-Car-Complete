/*
============================================================
        ESP32-S3 SMART ROBOT CAR
============================================================

WIFI
SSID     : Robot ESP 32
PASSWORD : 12345678
IP       : 192.168.4.1

HARDWARE
- ESP32-S3
- L298N
- 2 DC geared motors
- HC-SR04 ultrasonic
- Servo
- 2 IR obstacle sensors
- 2 PIR sensors
- Flame sensor
- Smoke sensor
- RC522 RFID
- 2 Hall sensors
- Buzzer
- Status LED

MODES
1. MANUAL
2. ROBOT / AUTONOMOUS

RFID AUTHORIZED UID
73 46 57 2D

============================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID = "RobotESP32";
const char* WIFI_PASSWORD = "12345678";

IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ============================================================
// PIN CONFIGURATION
// ============================================================
//
// IMPORTANT:
// Verify these GPIOs against your exact ESP32-S3 board.
//
// ============================================================

// ---------------- L298N LEFT MOTOR ----------------

const int L_IN1 = 4;
const int L_IN2 = 5;
const int L_EN  = 6;

// ---------------- L298N RIGHT MOTOR ----------------

const int R_IN1 = 7;
const int R_IN2 = 8;
const int R_EN  = 9;

// ---------------- ULTRASONIC ----------------

const int TRIG_PIN = 10;
const int ECHO_PIN = 11;

// ---------------- SERVO ----------------

const int SERVO_PIN = 12;

// ---------------- IR ----------------

const int IR_LEFT_PIN  = 13;
const int IR_RIGHT_PIN = 14;

// ---------------- PIR ----------------

const int PIR_LEFT_PIN  = 15;
const int PIR_RIGHT_PIN = 16;

// ---------------- FLAME ----------------

const int FLAME_PIN = 17;

// ---------------- SMOKE ----------------

const int SMOKE_PIN = 18;

// ---------------- RFID RC522 ----------------

const int RFID_SS_PIN   = 21;
const int RFID_RST_PIN  = 47;
const int RFID_SCK_PIN  = 36;
const int RFID_MISO_PIN = 37;
const int RFID_MOSI_PIN = 35;

// ---------------- HALL ----------------

const int HALL_LEFT_PIN  = 38;
const int HALL_RIGHT_PIN = 39;

// ---------------- BUZZER ----------------

const int BUZZER_PIN = 40;

// ---------------- STATUS LED ----------------

const int STATUS_LED_PIN = 41;

// ============================================================
// SENSOR LOGIC
// ============================================================

const int IR_ACTIVE_STATE = LOW;
const int PIR_ACTIVE_STATE = HIGH;
const int FLAME_ACTIVE_STATE = LOW;

// Smoke sensor threshold.
// Adjust after testing your actual sensor.
const int SMOKE_THRESHOLD = 1800;

// ============================================================
// DISTANCE SETTINGS
// ============================================================

const int OBSTACLE_STOP_CM = 20;
const int OBSTACLE_SAFE_CM = 35;

// ============================================================
// SERVO SETTINGS
// ============================================================

const int SERVO_LEFT_ANGLE   = 150;
const int SERVO_CENTER_ANGLE = 90;
const int SERVO_RIGHT_ANGLE  = 30;

// ============================================================
// HALL SENSOR SETTINGS
// ============================================================

const float WHEEL_DIAMETER_CM = 6.5;

// Change this according to your Hall encoder.
const int PULSES_PER_REV = 20;

// ============================================================
// RFID
// ============================================================

// Replace these placeholder bytes with your own RFID card UID.
// Do not publish your real UID in a public repository.
byte AUTHORIZED_UID[] = {
  0x00,
  0x00,
  0x00,
  0x00
};

const byte AUTHORIZED_UID_SIZE = 4;

// ============================================================
// OBJECTS
// ============================================================

MFRC522 rfid(
  RFID_SS_PIN,
  RFID_RST_PIN
);

// ============================================================
// STATES
// ============================================================

enum RobotMode {
  MANUAL_MODE,
  ROBOT_MODE
};

enum Movement {
  STOPPED,
  FORWARD_MOVEMENT,
  BACKWARD_MOVEMENT,
  LEFT_MOVEMENT,
  RIGHT_MOVEMENT
};

RobotMode robotMode = MANUAL_MODE;

Movement currentMovement = STOPPED;

// ============================================================
// GLOBAL VARIABLES
// ============================================================

int motorSpeed = 180;

bool emergencyStop = false;

bool rfidAuthorized = false;

bool flameDetected = false;
bool smokeDetected = false;

bool irLeftBlocked = false;
bool irRightBlocked = false;

bool pirLeftDetected = false;
bool pirRightDetected = false;

// Ultrasonic
long centerDistance = 999;
long leftDistance = 999;
long rightDistance = 999;

// Hall counters
volatile unsigned long hallLeftPulses = 0;
volatile unsigned long hallRightPulses = 0;

// RPM
float leftRPM = 0;
float rightRPM = 0;

// Travelled distance
float leftTravelCM = 0;
float rightTravelCM = 0;

// Timers
unsigned long lastCommandTime = 0;
unsigned long lastSensorTime = 0;
unsigned long lastRobotTime = 0;
unsigned long lastRPMTime = 0;
unsigned long lastRFIDTime = 0;

// Manual communication timeout
const unsigned long COMMAND_TIMEOUT = 1500;

// ============================================================
// HALL INTERRUPTS
// ============================================================

void IRAM_ATTR hallLeftISR() {
  hallLeftPulses++;
}

void IRAM_ATTR hallRightISR() {
  hallRightPulses++;
}

// ============================================================
// MOTOR CONTROL
// ============================================================

void setLeftMotor(int speedValue) {

  speedValue = constrain(
    speedValue,
    -255,
    255
  );

  if (speedValue > 0) {

    digitalWrite(L_IN1, HIGH);
    digitalWrite(L_IN2, LOW);

  }

  else if (speedValue < 0) {

    digitalWrite(L_IN1, LOW);
    digitalWrite(L_IN2, HIGH);

  }

  else {

    digitalWrite(L_IN1, LOW);
    digitalWrite(L_IN2, LOW);

  }

  ledcWrite(
    L_EN,
    abs(speedValue)
  );
}

// ------------------------------------------------------------

void setRightMotor(int speedValue) {

  speedValue = constrain(
    speedValue,
    -255,
    255
  );

  if (speedValue > 0) {

    digitalWrite(R_IN1, HIGH);
    digitalWrite(R_IN2, LOW);

  }

  else if (speedValue < 0) {

    digitalWrite(R_IN1, LOW);
    digitalWrite(R_IN2, HIGH);

  }

  else {

    digitalWrite(R_IN1, LOW);
    digitalWrite(R_IN2, LOW);

  }

  ledcWrite(
    R_EN,
    abs(speedValue)
  );
}

// ============================================================
// MOVEMENT
// ============================================================

void stopMotors() {

  setLeftMotor(0);
  setRightMotor(0);

  currentMovement = STOPPED;
}

// ------------------------------------------------------------

void moveForward() {

  if (emergencyStop) {
    stopMotors();
    return;
  }

  setLeftMotor(motorSpeed);
  setRightMotor(motorSpeed);

  currentMovement = FORWARD_MOVEMENT;
}

// ------------------------------------------------------------

void moveBackward() {

  if (emergencyStop) {
    stopMotors();
    return;
  }

  setLeftMotor(-motorSpeed);
  setRightMotor(-motorSpeed);

  currentMovement = BACKWARD_MOVEMENT;
}

// ------------------------------------------------------------

void turnLeft() {

  if (emergencyStop) {
    stopMotors();
    return;
  }

  setLeftMotor(-motorSpeed);
  setRightMotor(motorSpeed);

  currentMovement = LEFT_MOVEMENT;
}

// ------------------------------------------------------------

void turnRight() {

  if (emergencyStop) {
    stopMotors();
    return;
  }

  setLeftMotor(motorSpeed);
  setRightMotor(-motorSpeed);

  currentMovement = RIGHT_MOVEMENT;
}

// ============================================================
// ULTRASONIC
// ============================================================

long readUltrasonic() {

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  delayMicroseconds(3);

  digitalWrite(
    TRIG_PIN,
    HIGH
  );

  delayMicroseconds(10);

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  unsigned long duration =
    pulseIn(
      ECHO_PIN,
      HIGH,
      30000
    );

  if (duration == 0) {
    return 999;
  }

  long distance =
    (long)(duration * 0.0343 / 2.0);

  if (distance < 2 ||
      distance > 400) {

    return 999;
  }

  return distance;
}

// ============================================================
// SERVO PWM
// ============================================================

void setServoAngle(int angle) {

  angle = constrain(
    angle,
    0,
    180
  );

  // 50 Hz, 16-bit PWM
  // Approximate 500 us -> 1638
  // Approximate 2400 us -> 7864

  int pulse =
    map(
      angle,
      0,
      180,
      1638,
      7864
    );

  ledcWrite(
    SERVO_PIN,
    pulse
  );
}

// ============================================================
// SERVO SCAN
// ============================================================

long scanAtAngle(int angle) {

  setServoAngle(angle);

  delay(250);

  return readUltrasonic();
}

// ------------------------------------------------------------

void scanEnvironment() {

  leftDistance =
    scanAtAngle(
      SERVO_LEFT_ANGLE
    );

  centerDistance =
    scanAtAngle(
      SERVO_CENTER_ANGLE
    );

  rightDistance =
    scanAtAngle(
      SERVO_RIGHT_ANGLE
    );

  setServoAngle(
    SERVO_CENTER_ANGLE
  );
}

// ============================================================
// SENSOR READING
// ============================================================

void readSensors() {

  // IR

  irLeftBlocked =
    digitalRead(IR_LEFT_PIN)
    == IR_ACTIVE_STATE;

  irRightBlocked =
    digitalRead(IR_RIGHT_PIN)
    == IR_ACTIVE_STATE;

  // PIR

  pirLeftDetected =
    digitalRead(PIR_LEFT_PIN)
    == PIR_ACTIVE_STATE;

  pirRightDetected =
    digitalRead(PIR_RIGHT_PIN)
    == PIR_ACTIVE_STATE;

  // Flame

  flameDetected =
    digitalRead(FLAME_PIN)
    == FLAME_ACTIVE_STATE;

  // Smoke

  int smokeValue =
    analogRead(SMOKE_PIN);

  smokeDetected =
    smokeValue >= SMOKE_THRESHOLD;

  // Ultrasonic

  centerDistance =
    readUltrasonic();
}

// ============================================================
// SAFETY
// ============================================================

void checkSafety() {

  if (flameDetected ||
      smokeDetected) {

    emergencyStop = true;

    stopMotors();

    digitalWrite(
      BUZZER_PIN,
      HIGH
    );

    digitalWrite(
      STATUS_LED_PIN,
      HIGH
    );

  }
  else {

    digitalWrite(
      BUZZER_PIN,
      LOW
    );
  }
}

// ============================================================
// RFID
// ============================================================

bool readAuthorizedRFID() {

  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }

  bool authorized = true;

  if (rfid.uid.size !=
      AUTHORIZED_UID_SIZE) {

    authorized = false;

  }
  else {

    for (
      byte i = 0;
      i < AUTHORIZED_UID_SIZE;
      i++
    ) {

      if (
        rfid.uid.uidByte[i]
        != AUTHORIZED_UID[i]
      ) {

        authorized = false;
        break;
      }
    }
  }

  Serial.print(
    "RFID UID: "
  );

  for (
    byte i = 0;
    i < rfid.uid.size;
    i++
  ) {

    if (
      rfid.uid.uidByte[i] < 0x10
    ) {
      Serial.print("0");
    }

    Serial.print(
      rfid.uid.uidByte[i],
      HEX
    );

    if (
      i < rfid.uid.size - 1
    ) {
      Serial.print(" ");
    }
  }

  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return authorized;
}

// ------------------------------------------------------------

void processRFID() {

  if (
    millis() - lastRFIDTime
    < 250
  ) {
    return;
  }

  lastRFIDTime =
    millis();

  if (
    readAuthorizedRFID()
  ) {

    rfidAuthorized = true;

    emergencyStop = false;

    digitalWrite(
      BUZZER_PIN,
      LOW
    );

    digitalWrite(
      STATUS_LED_PIN,
      LOW
    );

    Serial.println(
      "RFID AUTHORIZED"
    );

  }
}

// ============================================================
// RPM
// ============================================================

void calculateRPM() {

  unsigned long now =
    millis();

  if (
    now - lastRPMTime
    < 1000
  ) {
    return;
  }

  unsigned long elapsed =
    now - lastRPMTime;

  lastRPMTime =
    now;

  noInterrupts();

  unsigned long leftPulses =
    hallLeftPulses;

  unsigned long rightPulses =
    hallRightPulses;

  hallLeftPulses = 0;
  hallRightPulses = 0;

  interrupts();

  float leftRevolutions =
    (float)leftPulses /
    PULSES_PER_REV;

  float rightRevolutions =
    (float)rightPulses /
    PULSES_PER_REV;

  float minutes =
    elapsed / 60000.0;

  if (minutes > 0) {

    leftRPM =
      leftRevolutions /
      minutes;

    rightRPM =
      rightRevolutions /
      minutes;
  }

  float circumference =
    PI * WHEEL_DIAMETER_CM;

  leftTravelCM +=
    leftRevolutions *
    circumference;

  rightTravelCM +=
    rightRevolutions *
    circumference;
}

// ============================================================
// AUTONOMOUS MODE
// ============================================================

void autonomousRobot() {

  if (emergencyStop) {

    stopMotors();
    return;
  }

  // --------------------------------------------
  // Front obstacle
  // --------------------------------------------

  if (
    centerDistance > 0 &&
    centerDistance < OBSTACLE_STOP_CM
  ) {

    stopMotors();

    delay(100);

    scanEnvironment();

    Serial.print(
      "LEFT: "
    );

    Serial.print(
      leftDistance
    );

    Serial.print(
      " CENTER: "
    );

    Serial.print(
      centerDistance
    );

    Serial.print(
      " RIGHT: "
    );

    Serial.println(
      rightDistance
    );

    if (
      leftDistance >
      OBSTACLE_SAFE_CM &&
      leftDistance >
      rightDistance
    ) {

      turnLeft();

      delay(400);

      stopMotors();

    }

    else if (
      rightDistance >
      OBSTACLE_SAFE_CM
    ) {

      turnRight();

      delay(400);

      stopMotors();

    }

    else {

      moveBackward();

      delay(400);

      turnRight();

      delay(600);

      stopMotors();
    }

    return;
  }

  // --------------------------------------------
  // IR
  // --------------------------------------------

  if (
    irLeftBlocked &&
    !irRightBlocked
  ) {

    turnRight();

    delay(250);

    stopMotors();

    return;
  }

  if (
    irRightBlocked &&
    !irLeftBlocked
  ) {

    turnLeft();

    delay(250);

    stopMotors();

    return;
  }

  if (
    irLeftBlocked &&
    irRightBlocked
  ) {

    moveBackward();

    delay(300);

    turnRight();

    delay(500);

    stopMotors();

    return;
  }

  // --------------------------------------------
  // Clear
  // --------------------------------------------

  moveForward();
}

// ============================================================
// STRING FUNCTIONS
// ============================================================

String getModeName() {

  if (
    robotMode ==
    ROBOT_MODE
  ) {
    return "ROBOT";
  }

  return "MANUAL";
}

// ------------------------------------------------------------

String getMovementName() {

  switch (
    currentMovement
  ) {

    case FORWARD_MOVEMENT:
      return "FORWARD";

    case BACKWARD_MOVEMENT:
      return "BACKWARD";

    case LEFT_MOVEMENT:
      return "LEFT";

    case RIGHT_MOVEMENT:
      return "RIGHT";

    default:
      return "STOP";
  }
}

// ============================================================
// WEB PAGE
// ============================================================

String makeWebPage() {

  String page = R"HTML(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width,initial-scale=1">

<title>Robot ESP 32</title>

<style>

body {
  background:#101010;
  color:#ffffff;
  font-family:Arial,sans-serif;
  text-align:center;
  margin:0;
  padding:12px;
}

h1 {
  margin:10px;
}

.panel {
  background:#202020;
  border-radius:16px;
  padding:15px;
  margin:12px auto;
  max-width:450px;
}

button {
  width:105px;
  height:55px;
  margin:5px;
  border:none;
  border-radius:12px;
  font-size:17px;
  font-weight:bold;
}

.control {
  background:#333333;
  color:white;
}

.stop {
  background:#d00000;
  color:white;
}

.mode {
  background:#444444;
  color:white;
}

.reset {
  background:#0066cc;
  color:white;
}

input[type=range] {
  width:90%;
}

.value {
  font-size:18px;
  margin:8px;
}

.good {
  color:#00ff66;
}

.bad {
  color:#ff3333;
}

</style>

<script>

function sendCommand(command) {

  fetch('/command?move=' + command);
}

function changeMode(mode) {

  fetch('/mode?value=' + mode);
}

function changeSpeed(value) {

  fetch('/speed?value=' + value);

  document.getElementById(
    'speedValue'
  ).innerText = value;
}

function resetEmergency() {

  fetch('/reset');
}

function updateStatus() {

  fetch('/status')

  .then(
    response => response.json()
  )

  .then(
    data => {

      document.getElementById(
        'mode'
      ).innerText = data.mode;

      document.getElementById(
        'movement'
      ).innerText = data.movement;

      document.getElementById(
        'speed'
      ).innerText = data.speed;

      document.getElementById(
        'center'
      ).innerText =
        data.center + ' cm';

      document.getElementById(
        'left'
      ).innerText =
        data.left + ' cm';

      document.getElementById(
        'right'
      ).innerText =
        data.right + ' cm';

      document.getElementById(
        'rpmLeft'
      ).innerText =
        data.rpmLeft;

      document.getElementById(
        'rpmRight'
      ).innerText =
        data.rpmRight;

      document.getElementById(
        'distanceLeft'
      ).innerText =
        data.travelLeft + ' cm';

      document.getElementById(
        'distanceRight'
      ).innerText =
        data.travelRight + ' cm';

      document.getElementById(
        'ir'
      ).innerText =
        data.ir;

      document.getElementById(
        'pir'
      ).innerText =
        data.pir;

      document.getElementById(
        'flame'
      ).innerText =
        data.flame;

      document.getElementById(
        'smoke'
      ).innerText =
        data.smoke;

      document.getElementById(
        'rfid'
      ).innerText =
        data.rfid;

      document.getElementById(
        'emergency'
      ).innerText =
        data.emergency;

    }
  );
}

setInterval(
  updateStatus,
  1000
);

</script>

</head>

<body>

<h1>🤖 Robot ESP 32</h1>

<p>
Smart Robot Car
</p>

<div class="panel">

<h2>MODE</h2>

<button
class="mode"
onclick="changeMode('manual')">

MANUAL

</button>

<button
class="mode"
onclick="changeMode('robot')">

ROBOT

</button>

</div>

<div class="panel">

<h2>CONTROL</h2>

<div>

<button
class="control"
onclick="sendCommand('forward')">

⬆

</button>

</div>

<div>

<button
class="control"
onclick="sendCommand('left')">

⬅

</button>

<button
class="stop"
onclick="sendCommand('stop')">

STOP

</button>

<button
class="control"
onclick="sendCommand('right')">

➡

</button>

</div>

<div>

<button
class="control"
onclick="sendCommand('backward')">

⬇

</button>

</div>

</div>

<div class="panel">

<h2>SPEED</h2>

<input
type="range"
min="0"
max="255"
value="180"
oninput="changeSpeed(this.value)">

<p>
Speed:
<b id="speedValue">
180
</b>
</p>

</div>

<div class="panel">

<h2>STATUS</h2>

<div class="value">
Mode:
<b id="mode">
MANUAL
</b>
</div>

<div class="value">
Movement:
<b id="movement">
STOP
</b>
</div>

<div class="value">
Speed:
<b id="speed">
180
</b>
</div>

<div class="value">
Emergency:
<b id="emergency">
NO
</b>
</div>

<div class="value">
RFID:
<b id="rfid">
NOT AUTHORIZED
</b>
</div>

</div>

<div class="panel">

<h2>ULTRASONIC</h2>

<div class="value">
Center:
<b id="center">
---
</b>
</div>

<div class="value">
Left:
<b id="left">
---
</b>
</div>

<div class="value">
Right:
<b id="right">
---
</b>
</div>

</div>

<div class="panel">

<h2>HALL / RPM</h2>

<div class="value">
Left RPM:
<b id="rpmLeft">
0
</b>
</div>

<div class="value">
Right RPM:
<b id="rpmRight">
0
</b>
</div>

<div class="value">
Left Distance:
<b id="distanceLeft">
0 cm
</b>
</div>

<div class="value">
Right Distance:
<b id="distanceRight">
0 cm
</b>
</div>

</div>

<div class="panel">

<h2>IR</h2>

<div class="value">
<b id="ir">
CLEAR
</b>
</div>

<h2>PIR</h2>

<div class="value">
<b id="pir">
CLEAR
</b>
</div>

</div>

<div class="panel">

<h2>SAFETY</h2>

<div class="value">
Flame:
<b id="flame">
CLEAR
</b>
</div>

<div class="value">
Smoke:
<b id="smoke">
CLEAR
</b>
</div>

<button
class="reset"
onclick="resetEmergency()">

RESET EMERGENCY

</button>

</div>

</body>

</html>

)HTML";

  return page;
}

// ============================================================
// WEB ROOT
// ============================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    makeWebPage()
  );
}

// ============================================================
// WEB COMMAND
// ============================================================

void handleCommand() {

  if (
    !server.hasArg("move")
  ) {

    server.send(
      400,
      "text/plain",
      "Missing command"
    );

    return;
  }

  lastCommandTime =
    millis();

  if (
    robotMode ==
    ROBOT_MODE
  ) {

    server.send(
      200,
      "text/plain",
      "ROBOT MODE ACTIVE"
    );

    return;
  }

  if (emergencyStop) {

    stopMotors();

    server.send(
      200,
      "text/plain",
      "EMERGENCY STOP ACTIVE"
    );

    return;
  }

  String command =
    server.arg("move");

  if (
    command == "forward"
  ) {

    moveForward();

  }

  else if (
    command == "backward"
  ) {

    moveBackward();

  }

  else if (
    command == "left"
  ) {

    turnLeft();

  }

  else if (
    command == "right"
  ) {

    turnRight();

  }

  else {

    stopMotors();
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// WEB MODE
// ============================================================

void handleMode() {

  if (
    !server.hasArg("value")
  ) {

    server.send(
      400,
      "text/plain",
      "Missing mode"
    );

    return;
  }

  String mode =
    server.arg("value");

  stopMotors();

  if (
    mode == "robot"
  ) {

    robotMode =
      ROBOT_MODE;

    Serial.println(
      "ROBOT MODE"
    );

  }

  else {

    robotMode =
      MANUAL_MODE;

    Serial.println(
      "MANUAL MODE"
    );
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// WEB SPEED
// ============================================================

void handleSpeed() {

  if (
    !server.hasArg("value")
  ) {

    server.send(
      400,
      "text/plain",
      "Missing speed"
    );

    return;
  }

  motorSpeed =
    constrain(
      server.arg("value").toInt(),
      0,
      255
    );

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// WEB RESET
// ============================================================

void handleReset() {

  bool flame =
    digitalRead(FLAME_PIN)
    == FLAME_ACTIVE_STATE;

  bool smoke =
    analogRead(SMOKE_PIN)
    >= SMOKE_THRESHOLD;

  if (
    flame ||
    smoke
  ) {

    emergencyStop = true;

    stopMotors();

    server.send(
      200,
      "text/plain",
      "SAFETY SENSOR STILL ACTIVE"
    );

    return;
  }

  emergencyStop = false;

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  digitalWrite(
    STATUS_LED_PIN,
    LOW
  );

  server.send(
    200,
    "text/plain",
    "EMERGENCY RESET"
  );
}

// ============================================================
// WEB STATUS
// ============================================================

void handleStatus() {

  String json = "{";

  json += "\"mode\":\"";
  json += getModeName();
  json += "\",";

  json += "\"movement\":\"";
  json += getMovementName();
  json += "\",";

  json += "\"speed\":";
  json += motorSpeed;
  json += ",";

  json += "\"center\":";
  json += centerDistance;
  json += ",";

  json += "\"left\":";
  json += leftDistance;
  json += ",";

  json += "\"right\":";
  json += rightDistance;
  json += ",";

  json += "\"rpmLeft\":";
  json += String(leftRPM, 1);
  json += ",";

  json += "\"rpmRight\":";
  json += String(rightRPM, 1);
  json += ",";

  json += "\"travelLeft\":";
  json += String(leftTravelCM, 1);
  json += ",";

  json += "\"travelRight\":";
  json += String(rightTravelCM, 1);
  json += ",";

  json += "\"ir\":\"";

  if (
    irLeftBlocked ||
    irRightBlocked
  ) {

    json += "BLOCKED";

  } else {

    json += "CLEAR";
  }

  json += "\",";

  json += "\"pir\":\"";

  if (
    pirLeftDetected ||
    pirRightDetected
  ) {

    json += "MOTION";

  } else {

    json += "CLEAR";
  }

  json += "\",";

  json += "\"flame\":\"";

  if (flameDetected) {

    json += "DETECTED";

  } else {

    json += "CLEAR";
  }

  json += "\",";

  json += "\"smoke\":\"";

  if (smokeDetected) {

    json += "DETECTED";

  } else {

    json += "CLEAR";
  }

  json += "\",";

  json += "\"rfid\":\"";

  if (rfidAuthorized) {

    json += "AUTHORIZED";

  } else {

    json += "NOT AUTHORIZED";
  }

  json += "\",";

  json += "\"emergency\":\"";

  if (emergencyStop) {

    json += "ACTIVE";

  } else {

    json += "NO";
  }

  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// WIFI
// ============================================================

void startWiFi() {

  Serial.println();
  Serial.println(
    "Starting WiFi..."
  );

  WiFi.mode(WIFI_AP);

  WiFi.setSleep(false);

  bool configOK =
    WiFi.softAPConfig(
      localIP,
      gateway,
      subnet
    );

  if (configOK) {

    Serial.println(
      "WiFi AP configuration OK"
    );

  } else {

    Serial.println(
      "WiFi AP configuration FAILED"
    );
  }

  bool wifiOK =
    WiFi.softAP(
      WIFI_SSID,
      WIFI_PASSWORD,
      6,
      false,
      4
    );

  if (wifiOK) {

    Serial.println();
    Serial.println(
      "=============================="
    );

    Serial.println(
      "       WIFI AP STARTED"
    );

    Serial.println(
      "=============================="
    );

    Serial.print(
      "SSID: "
    );

    Serial.println(
      WIFI_SSID
    );

    Serial.print(
      "Password: "
    );

    Serial.println(
      WIFI_PASSWORD
    );

    Serial.print(
      "IP: "
    );

    Serial.println(
      WiFi.softAPIP()
    );

    Serial.print(
      "MAC: "
    );

    Serial.println(
      WiFi.softAPmacAddress()
    );

    Serial.println(
      "=============================="
    );

  } else {

    Serial.println(
      "ERROR: WIFI AP FAILED"
    );
  }
}

// ============================================================
// HARDWARE SETUP
// ============================================================

void setupHardware() {

  // Motors

  pinMode(
    L_IN1,
    OUTPUT
  );

  pinMode(
    L_IN2,
    OUTPUT
  );

  pinMode(
    R_IN1,
    OUTPUT
  );

  pinMode(
    R_IN2,
    OUTPUT
  );

  // Ultrasonic

  pinMode(
    TRIG_PIN,
    OUTPUT
  );

  pinMode(
    ECHO_PIN,
    INPUT
  );

  // IR

  pinMode(
    IR_LEFT_PIN,
    INPUT
  );

  pinMode(
    IR_RIGHT_PIN,
    INPUT
  );

  // PIR

  pinMode(
    PIR_LEFT_PIN,
    INPUT
  );

  pinMode(
    PIR_RIGHT_PIN,
    INPUT
  );

  // Flame

  pinMode(
    FLAME_PIN,
    INPUT
  );

  // Smoke

  pinMode(
    SMOKE_PIN,
    INPUT
  );

  // Hall

  pinMode(
    HALL_LEFT_PIN,
    INPUT_PULLUP
  );

  pinMode(
    HALL_RIGHT_PIN,
    INPUT_PULLUP
  );

  // Buzzer

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  // LED

  pinMode(
    STATUS_LED_PIN,
    OUTPUT
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  digitalWrite(
    STATUS_LED_PIN,
    LOW
  );
}

// ============================================================
// PWM SETUP
// ============================================================

void setupPWM() {

  // L298N PWM

  ledcAttach(
    L_EN,
    20000,
    8
  );

  ledcAttach(
    R_EN,
    20000,
    8
  );

  ledcWrite(
    L_EN,
    0
  );

  ledcWrite(
    R_EN,
    0
  );

  // Servo PWM

  ledcAttach(
    SERVO_PIN,
    50,
    16
  );

  setServoAngle(
    SERVO_CENTER_ANGLE
  );
}

// ============================================================
// RFID SETUP
// ============================================================

void setupRFID() {

  SPI.begin(
    RFID_SCK_PIN,
    RFID_MISO_PIN,
    RFID_MOSI_PIN,
    RFID_SS_PIN
  );

  rfid.PCD_Init();

  delay(50);

  Serial.println(
    "RFID initialized"
  );
}

// ============================================================
// WEB SERVER SETUP
// ============================================================

void setupWebServer() {

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/command",
    HTTP_GET,
    handleCommand
  );

  server.on(
    "/mode",
    HTTP_GET,
    handleMode
  );

  server.on(
    "/speed",
    HTTP_GET,
    handleSpeed
  );

  server.on(
    "/reset",
    HTTP_GET,
    handleReset
  );

  server.on(
    "/status",
    HTTP_GET,
    handleStatus
  );

  server.begin();

  Serial.println(
    "Web server started"
  );
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(1500);

  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    "    ESP32-S3 SMART ROBOT CAR"
  );
  Serial.println(
    "================================"
  );

  // Hardware

  Serial.println(
    "Initializing hardware..."
  );

  setupHardware();

  // PWM

  Serial.println(
    "Initializing PWM..."
  );

  setupPWM();

  stopMotors();

  // RFID

  Serial.println(
    "Initializing RFID..."
  );

  setupRFID();

  // Hall

  attachInterrupt(
    digitalPinToInterrupt(
      HALL_LEFT_PIN
    ),
    hallLeftISR,
    FALLING
  );

  attachInterrupt(
    digitalPinToInterrupt(
      HALL_RIGHT_PIN
    ),
    hallRightISR,
    FALLING
  );

  // Wi-Fi

  startWiFi();

  // Web

  setupWebServer();

  lastCommandTime =
    millis();

  lastRPMTime =
    millis();

  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    "          ROBOT READY"
  );
  Serial.println(
    "================================"
  );

  Serial.println(
    "WiFi: Robot ESP 32"
  );

  Serial.println(
    "Password: 12345678"
  );

  Serial.println(
    "IP: http://192.168.4.1"
  );

  Serial.println(
    "================================"
  );
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // Web server

  server.handleClient();

  // RFID

  processRFID();

  // Sensors

  if (
    millis() - lastSensorTime
    >= 150
  ) {

    lastSensorTime =
      millis();

    readSensors();

    checkSafety();
  }

  // RPM

  calculateRPM();

  // Manual watchdog

  if (
    robotMode ==
    MANUAL_MODE
  ) {

    if (
      millis() - lastCommandTime
      > COMMAND_TIMEOUT
    ) {

      stopMotors();
    }
  }

  // Robot mode

  if (
    robotMode ==
    ROBOT_MODE
  ) {

    if (
      millis() - lastRobotTime
      >= 100
    ) {

      lastRobotTime =
        millis();

      autonomousRobot();
    }
  }

  // Emergency always wins

  if (emergencyStop) {

    stopMotors();

    digitalWrite(
      STATUS_LED_PIN,
      HIGH
    );

    digitalWrite(
      BUZZER_PIN,
      HIGH
    );

  }

  else {

    digitalWrite(
      STATUS_LED_PIN,
      LOW
    );

    digitalWrite(
      BUZZER_PIN,
      LOW
    );
  }

  delay(2);
}