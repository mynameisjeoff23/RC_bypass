# RC Bypass

ESP32-C3 Wi-Fi controller for a two-H-bridge RC car.

This project is intended for a **2 H-bridge RC car with only two functions**:

- Steering: left, center, and right
- Throttle: forward, stop, and reverse

The ESP32-C3 creates its own Wi-Fi access point and serves a browser-based controller. No internet connection, router, mobile application, or traditional RC receiver is required.

## Features

- ESP32-C3 DevKitM-1 firmware
- Built-in Wi-Fi access point
- Browser-based control interface
- Throttle control from -100% to +100%
- Steering control with left, center, and right positions
- PWM motor control at 5 kHz
- Automatic stop when the controller disconnects from Wi-Fi
- Small startup boost to help the drive motor begin turning

## Hardware Required

- ESP32-C3-DevKitM-1
- Two H-bridge motor drivers, or one dual H-bridge driver
    - The RC car's existing hardware can be tapped into. Doing so requires
    matching with the RC car chip's pinout.
- RC car with:
  - One drive motor
  - One steering motor
- Suitable battery for the motors and motor drivers
- 5 V or 3.3 V regulated supply for the ESP32-C3, depending on the board and wiring
- Common ground between the ESP32-C3 and the H-bridge logic ground
- USB cable for programming

## Important Safety Information

Test the car with the drive wheels raised off the ground before installing the electronics.

The ESP32 GPIO pins must connect only to H-bridge **logic inputs**. Do not connect a motor, battery, or other high-current load directly to an ESP32 GPIO.

Use a motor supply appropriate for the motors and H-bridge. Do not power the motors from the ESP32 board.

Before testing:

1. Remove the car wheels from the ground or place the car on a stand.
2. Start with the throttle at zero.
3. Verify the H-bridge enable and standby pins are configured correctly.
4. Confirm that the motor driver logic voltage is compatible with the ESP32-C3.
5. Disconnect the battery if the motor driver or wiring becomes hot.

This project does not provide current limiting, collision detection, battery monitoring, or emergency-stop hardware. The Wi-Fi disconnect stop behavior should not be treated as a replacement for a physical emergency stop.

## Pin Assignment

The firmware is configured for an ESP32-C3-DevKitM-1.

| ESP32-C3 GPIO | Firmware name | Function |
|---|---|---|
| GPIO 3 | `PIN_DRIVE_FWD` | Drive H-bridge input controlled by negative throttle |
| GPIO 4 | `PIN_DRIVE_REV` | Drive H-bridge input controlled by positive throttle |
| GPIO 8 | `PIN_STEER_LEFT` | Steering H-bridge left input |
| GPIO 9 | `PIN_STEER_RIGHT` | Steering H-bridge right input |
| GND | — | Common logic ground |

The firmware uses GPIO 3 and GPIO 4 as the two direction/PWM inputs for the drive H-bridge. GPIO 8 and GPIO 9 are digital outputs for the steering H-bridge.

> Verify the actual direction of your motor driver before permanently installing the wiring. If forward and reverse are reversed, swap the motor wires or change the firmware mapping.

### H-Bridge Connections

The exact labels depend on the motor-driver module. The following table describes the logical connections:

| ESP32-C3 | Drive H-bridge | Steering H-bridge |
|---|---|---|
| GPIO 3 | Drive direction/PWM input 1 | — |
| GPIO 4 | Drive direction/PWM input 2 | — |
| GPIO 8 | — | Steering left input |
| GPIO 9 | — | Steering right input |
| GND | Logic GND | Logic GND |
| Motor battery | Motor power input | Motor power input |
| Drive motor | Drive motor output | — |
| Steering motor | — | Steering motor output |

Connect any required H-bridge `EN`, `STBY`, or enable pins according to the driver documentation. If the driver requires an enable pin to be held high, connect it to the appropriate logic supply or add a dedicated ESP32 control pin.

Do not connect the two H-bridge motor outputs together.

## Project Layout

```text
.
├── platformio.ini
├── CMakeLists.txt
├── sdkconfig.esp32-c3-devkitm-1
├── src/
│   ├── main.c
│   ├── webapp_html.h
│   └── webapp_js.h
├── managed_components/
│   └── espressif__mdns/
└── test/
```

The firmware is implemented in `src/main.c`. The browser interface is embedded
in the firmware through `src/webapp_html.h` and `src/webapp_js.h`.

## Setting Up the Development Environment

### VS Code and PlatformIO

1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. Install the PlatformIO IDE extension.
3. Open this project folder in VS Code:

   ```text
   D:\Arduino\RC_bypass\RC_bypass
   ```

4. Connect the ESP32-C3-DevKitM-1 with a USB data cable.
5. Allow PlatformIO to install the Espressif32 platform, ESP-IDF framework,
   and project dependencies.

The project is configured for the `esp32-c3-devkitm-1` board and the ESP-IDF
framework in `platformio.ini`.

### Recommended VS Code Integrated Workflow

PlatformIO command-line (`pio`) commands do not always work reliably in every
VS Code terminal or Windows environment. When a `pio` command is not found,
fails to start, or fails while PlatformIO is otherwise installed, use the
PlatformIO buttons and project tasks built into VS Code instead.

1. Open the **PlatformIO** icon in the VS Code Activity Bar. It looks like an
   alien-head logo.
2. Expand **PROJECT TASKS**.
3. Expand **esp32-c3-devkitm-1**.
4. Use the integrated tasks:

   - **Build**: compile the firmware.
   - **Upload**: flash the firmware to the connected ESP32-C3.
   - **Clean**: remove the previous build output if a clean rebuild is needed.
   - **Monitor**: open the serial monitor at the configured 115200 baud.
   - **Erase Flash**: erase the board before uploading when recovery is needed.

   Depending on the PlatformIO extension version, the same actions may also be
   available as checkmark/build, upload-arrow, trash/clean, and serial-monitor
   buttons in the PlatformIO toolbar or at the bottom of the VS Code window.

5. If more than one serial port is available, select the ESP32-C3 upload port
   when PlatformIO prompts for it.

The integrated **Upload** task is the preferred way to flash this project when
the terminal `pio` commands fail.

### PlatformIO Command Line

The commands below are available when PlatformIO Core is correctly available
in the terminal. From the project directory, run:

```powershell
pio run
```

This builds the firmware and confirms that the environment is installed
correctly. If this command fails with a command-not-found or PlatformIO
environment error, use the VS Code integrated tasks described above instead
of repeatedly retrying the command.

## Build and Flash

Connect the ESP32-C3 to USB, then use the PlatformIO **Build** task followed by
the **Upload** task under **PROJECT TASKS > esp32-c3-devkitm-1**. The
integrated buttons are recommended because `pio` commands often do not work
from the VS Code terminal on Windows.

If the command line is working, the equivalent commands are:

```powershell
pio run
pio run --target upload
```

If PlatformIO does not select the correct serial port automatically, specify
it explicitly:

```powershell
pio run --target upload --upload-port COM5
```

Replace `COM5` with the port assigned to your board.

To erase the ESP32 flash before uploading:

```powershell
pio run --target erase
pio run --target upload
```

Erasing is normally unnecessary, but can help recover from corrupted flash
data or configuration changes.

To erase flash with the VS Code interface, open **PlatformIO > PROJECT TASKS >
esp32-c3-devkitm-1** and select **Erase Flash**, then select **Upload**.

## Serial Monitor

The firmware uses 115200 baud:

The preferred VS Code method is **PlatformIO > PROJECT TASKS >
esp32-c3-devkitm-1 > Monitor**, or the PlatformIO serial-monitor button.

If the command line is working, the equivalent command is:

```powershell
pio device monitor
```

To specify a port:

```powershell
pio device monitor --port COM5 --baud 115200
```

After startup, the log should report:

```text
Connect to RC-Car and open http://car.local
```

## Wiring

The ESP32 pins connect to the **logic inputs** of the H-bridges. Never connect
a motor or motor-battery voltage directly to an ESP32 GPIO.

| ESP32-C3 | Drive H-bridge | Steering H-bridge |
|---|---|---|
| GPIO 3 | Drive input 1 / negative-throttle PWM | — |
| GPIO 4 | Drive input 2 / positive-throttle PWM | — |
| GPIO 8 | — | Steering left input |
| GPIO 9 | — | Steering right input |
| GND | Logic ground | Logic ground |

Also connect:

- The drive motor to the drive H-bridge motor output.
- The steering motor to the steering H-bridge motor output.
- The motor battery to the motor-power inputs of the H-bridges.
- Any required `EN` or `STBY` pins according to the H-bridge documentation.
- A common ground between the ESP32 and both H-bridge logic grounds.

Use a separate, suitable motor supply. Do not power the motors from the
ESP32 board. Confirm that the H-bridge logic voltage is compatible with the
ESP32-C3.

The project is for a car with two H-bridge-controlled functions only:
one drive motor and one steering motor. It does not control lights, braking,
servos, suspension, or additional accessories.

## Using the Car

1. Keep the drive wheels raised during the first test.
2. Flash the firmware and connect the motor-driver battery.
3. Power or reset the ESP32-C3.
4. Connect your phone, tablet, or computer to the Wi-Fi network:

   ```text
   SSID: RC-Car
   Password: rc-car-control
   ```

5. Open the controller in a browser:

   ```text
   http://car.local
   ```

   If the hostname does not resolve, open:

   ```text
   http://192.168.4.1
   ```

6. Use the throttle slider and steering buttons to control the car.

### Throttle

Throttle ranges from `-100` to `100`:

- `0`: stopped
- Positive values: one drive direction
- Negative values: the opposite drive direction

The exact forward direction depends on motor wiring. Verify it with the
wheels raised before driving. Any non-zero throttle below 50% is raised to
50% to help the motor start, and a short startup boost is applied when
starting from stopped.

### Steering

The steering controls are left, center, and right. Only one steering direction
is enabled at a time. Releasing the control returns the steering output to
center.

### Disconnect Safety Behavior

If the controller disconnects from the ESP32 Wi-Fi access point, the firmware
sets throttle to zero and steering to center. This is a software fallback,
not a substitute for a physical emergency stop.

## Troubleshooting

### The car moves in the wrong direction

Swap the drive motor wires at the H-bridge output, or change the direction
mapping in `src/main.c`.

### Steering is reversed

Swap the steering motor wires, or change the left/right GPIO mapping in
`src/main.c`.

### A motor does not move

Check the following:

- ESP32 and H-bridge grounds are connected.
- The H-bridge logic supply and motor supply are present.
- Required `EN` and `STBY` pins are enabled.
- GPIO 3, GPIO 4, GPIO 8, and GPIO 9 are connected to the intended inputs.
- The motor driver accepts the ESP32-C3 logic voltage.
- The motor is connected to the H-bridge output, not an ESP32 GPIO.

### The Wi-Fi page does not open

- Confirm that the device is connected to `RC-Car`.
- Try `http://192.168.4.1` instead of `http://car.local`.
- Check the serial monitor for startup errors.
- Reset the ESP32 and reconnect to the access point.

## Customization

The main configuration values are in `src/main.c`:

```c
#define PIN_DRIVE_FWD    3
#define PIN_DRIVE_REV    4
#define PIN_STEER_LEFT   8
#define PIN_STEER_RIGHT  9

#define LEDC_FREQUENCY   5000
#define MIN_DRIVE_SPEED_PERCENT 50

#define AP_SSID          "RC-Car"
#define AP_PASSWORD      "rc-car-control"
```

After changing the firmware, rebuild and upload it:

```powershell
pio run --target upload
```

## Hardware Test Procedure

Test in this order:

1. ESP32 powered by USB with no motor battery connected.
2. H-bridge logic connected with motors disconnected.
3. Motors connected with wheels raised.
4. Low-speed operation on the ground.
5. Full operation after direction and disconnect-stop behavior are confirmed.

## License

This project is released under the Unlicense. See [LICENSE](LICENSE).