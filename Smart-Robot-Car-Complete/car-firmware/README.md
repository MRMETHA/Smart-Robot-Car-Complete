# ESP32-S3 Smart Robot Car Firmware

## Included features from the supplied sketch

- ESP32-S3 Wi-Fi access point and HTTP control server
- L298N dual DC motor control
- Manual and autonomous modes
- HC-SR04 ultrasonic distance measurement with servo scanning
- Left and right IR obstacle sensors
- Left and right PIR sensors
- Flame and smoke sensor monitoring
- RC522 RFID authentication logic
- Two Hall sensors for wheel pulse / RPM calculation
- Buzzer, status LED and emergency-stop state
- Browser dashboard and JSON status endpoint

## Arduino IDE setup

1. Install Arduino IDE and the ESP32 board package.
2. Select the exact ESP32-S3 board model and its correct USB/flash settings.
3. Install the `MFRC522` library from Library Manager.
4. Open `src/SmartRobotCar.ino`.
5. Verify the GPIO map against your physical board and wiring.
6. Replace the placeholder RFID UID with your own UID locally.
7. Upload and open Serial Monitor at 115200 baud.

## Wi-Fi dashboard

The sketch creates an access point with example settings:

- SSID: `RobotESP32`
- Password: `12345678` (change this before use)
- Dashboard: `http://192.168.4.1`

## HTTP routes

- `/` – robot control page
- `/command?move=forward|backward|left|right|stop`
- `/mode?value=manual|robot`
- `/speed?value=0..255`
- `/reset`
- `/status` – JSON status

## Important hardware checks

- Confirm the GPIO numbers suit your exact ESP32-S3 board.
- HC-SR04 ECHO can output 5 V. Use a level shifter or divider before ESP32 GPIO.
- Power motors from an appropriate motor supply, not from the ESP32 5 V pin.
- Join all grounds appropriately.
- Test motor direction and emergency stop with the wheels lifted before floor testing.
- Software emergency stop is not a substitute for a physical power disconnect.

The supplied code's autonomous behaviour is basic obstacle avoidance, not AI mapping or path planning.
