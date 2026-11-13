# Integration notes

## Two separate services

1. ESP32-S3 control service: `http://192.168.4.1` when connected to the robot's AP.
2. Android front-camera service: `http://PHONE_IP:8080` when laptop and phone share a network.

These addresses are not automatically combined. The present ESP32 sketch uses AP mode; the Android app serves the camera feed from the phone. To show both on one laptop page, the laptop must have routes to both devices. Easiest practical arrangement is to modify ESP32 Wi-Fi to station mode and connect it to the same phone hotspot/router as the phone and laptop. The supplied car sketch has not been rewritten to change its networking mode.

## Project photo

See `robot-and-laptop-setup.jpg` for the uploaded prototype reference.
