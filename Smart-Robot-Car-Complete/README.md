# Smart Robot Car – Complete Project

This repository keeps the **ESP32-S3 robot car firmware** and the **Android robot-eyes / front-camera app** in separate folders.

## Repository layout

```text
Smart-Robot-Car-Complete/
├── README.md
├── car-firmware/
│   ├── README.md
│   ├── src/
│   │   └── SmartRobotCar.ino
│   └── docs/
│       ├── diagrams/
│       └── project-photos/
├── android-app/
│   ├── README.md
│   ├── settings.gradle
│   ├── build.gradle
│   ├── gradle.properties
│   └── app/
│       └── src/main/
│           ├── AndroidManifest.xml
│           ├── java/com/example/roboteyes/
│           │   ├── MainActivity.java
│           │   └── RobotEyesView.java
│           └── res/values/
└── docs/
    └── robot-and-laptop-setup.jpg
```

## System overview

- ESP32-S3 controls the robot car hardware and serves its own Wi-Fi control dashboard.
- The Android app displays animated eyes in landscape orientation.
- The Android app requests front-camera permission and captures frames using CameraX.
- The app runs a small local HTTP server on port `8080`.
- Open the phone's displayed URL on a laptop browser to see the front-camera dashboard.

## Important network note

The current car firmware starts its own Wi-Fi access point (`RobotESP32`, default example password `12345678`, address `192.168.4.1`). The phone camera server and ESP32 access point are separate network services. A laptop and phone must share a reachable Wi-Fi network to view the phone camera page. If the ESP32 remains in access-point mode, the laptop may not be able to reach both the phone camera server and the ESP32 dashboard at the same time. For a unified dashboard, configure the ESP32 to join the same router/hotspot as the phone, or use a second network interface. Do not assume the two dashboards are automatically merged.

## Safety and configuration

- Verify every ESP32-S3 GPIO against the exact board revision before connecting hardware.
- Use a suitable voltage divider/level shifter on HC-SR04 ECHO if the module outputs 5 V; ESP32 GPIO is not 5 V tolerant.
- Use a separate, adequately rated motor supply and a common ground with the controller.
- Test wheels off the ground first and keep a physical power disconnect available.
- The RFID UID in the public sketch is replaced with a placeholder. Enter your own card UID locally and do not publish it.
- Change the example Wi-Fi password before real use.
- The existing sketch has basic obstacle-avoidance logic; it is not SLAM, mapping, or advanced AI navigation.
- The Android camera server is intended for a trusted local network. It has no login or encryption; do not expose port 8080 to the public internet.

## Build

See `car-firmware/README.md` and `android-app/README.md` for separate setup instructions.
