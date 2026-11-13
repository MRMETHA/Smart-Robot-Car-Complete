# Android Robot Eyes + Front Camera Dashboard

## What this app does

- Landscape, full-screen robot eyes animation (retained from your uploaded app)
- Automatic eye movement and blinking
- Tap to pause/resume the automatic eye movement; swipe to change gaze direction
- Uses the phone's **front camera** through CameraX after permission is granted
- Hosts a local web dashboard on port `8080`
- Laptop browser displays the latest camera frame, refreshed several times per second

## Open in Android Studio

1. Extract/open the `android-app` folder in Android Studio.
2. Allow Android Studio to download the Android Gradle Plugin, Gradle 8.9, and Android SDK 35 if prompted.
3. Connect your Android phone with USB debugging enabled.
4. Run the `app` configuration.
5. Grant Camera permission when asked.
6. Connect the phone and laptop to the same Wi-Fi network.
7. Read the URL displayed at the bottom of the phone screen, for example `http://PHONE_IP:8080`.
8. Open that URL on the laptop browser.

## Endpoints

- `/` – laptop camera dashboard
- `/snapshot.jpg` – latest JPEG frame

## Notes

- This implementation uses CameraX ImageAnalysis and a lightweight NanoHTTPD server.
- The app's eye animation stays on the phone screen while camera frames are sent to the laptop.
- The phone and laptop must be on the same reachable local network.
- The stream has no authentication or encryption. Use only on a trusted local network; do not port-forward it to the internet.
- Battery and heat use can increase while the camera is active.
- This source has not been built on your exact Android Studio / phone combination in this environment. Build and test it on the target phone; if Gradle reports an error, share the full Build Output.

## Original uploaded app

The eye animation implementation was retained from the uploaded `RobotEyesLandscape.zip`; camera and local dashboard support have been added around it.
