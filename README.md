# Carwash Vendo — Android Kiosk & ESP32 Controller

Complete Android 10+ (Jetpack Compose + USB Serial) kiosk application and ESP32/Arduino firmware for a coin-operated self-service carwash machine.

## Repository Structure

- `android-project/` — Complete Kotlin + Jetpack Compose Android Studio project (`minSdk = 29`, `usb-serial-for-android:3.8.0`)
- `firmware/carwash_vendo_esp32.ino` — ESP32/Arduino sketch for multi-coin pulse interrupts, active-LOW optocoupler relays, physical buttons, and 3.5s safety watchdog
- `.github/workflows/build-apk.yml` — Automated GitHub Actions workflow that compiles `app-debug.apk` in the cloud

## Default Rates & Admin PIN

- **Water Rinse:** ₱5 = 2 minutes (`120s`)
- **Foam Soap:** ₱5 = 1 minute (`60s`)
- **Air Blower:** ₱5 = 2 minutes (`120s`)
- **Admin Access:** Tap the kiosk title **5 times** and enter PIN `1234`

## Building the APK on GitHub (No Android Studio Needed)

1. Ensure `.github/workflows/build-apk.yml`, `android-project/`, and `firmware/` are at the root of your GitHub repository.
2. Open the **Actions** tab in your GitHub repository.
3. Wait for **Build Carwash Vendo APK** to finish (green checkmark, ~2 minutes).
4. Open the completed run, scroll to **Artifacts**, and click **`carwash-vendo-debug-apk`** to download `app-debug.apk`.
