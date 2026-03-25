# ESP32 Car project
Web interface controlling the ESP32 via websocket connection.
The ESP32 is connected to a motor driver which controls the motors of the car. The web interface allows the user to control the car's movement and speed.
One motor driver for wheels and one motor driver for the steering. The car is powered by a battery pack and can be controlled remotely via Wi-Fi.


### Frameworks and libraries used:
IDE: VSCode with PlatformIO extension for development and debugging.
Arduino framework for ESP32. Run commands with `~/.platformio/penv/bin/pio run ...`.
ESPAsyncWebServer library for handling the web server and websocket connections.
(https://github.com/ESP32Async/ESPAsyncWebServer)
AsyncTCP library for handling TCP connections.


### Board and components used:
ESP32-DevKitC - ESP32-WROOM-32D
BTS7960 motor driver for wheels
DRV8871 H-Bridge Motor Driver for steering
Analog foot pedal for speed control
12v battery pack 14Ah

### Features:
- Web interface for controlling the car's movement and speed served by the ESP32.
- Websocket connection for real-time control of the car.
- Motor control for both wheels and steering.
- Speed control using an analog foot pedal.
- Battery-powered for remote operation.

### Pinout:
- GPIO25: PWM signal for FORWARD motor speed control (BTS7960)
- GPIO26: PWM signal for BACKWARD motor speed control (BTS7960)
- GPIO32: PWM signal for LEFT steering control (DRV8871)
- GPIO33: PWM signal for RIGHT steering control (DRV8871)
- GPIO36: Analog input for foot pedal speed control

#### BTS7960 Motor Driver:
Uses a fixed PWM frequency of 20kHz (or a ESP32 friendly frequency) for controlling the speed of the motors.
The duty cycle of the PWM signal determines the speed of the motors, with 0% being stopped and 100% being full speed.
Only one of the FORWARD or BACKWARD signals should be active at a time to prevent damage to the motor driver.

#### DRV8871 H-Bridge Motor Driver:
Uses a fixed PWM frequency of 20kHz (or a ESP32 friendly frequency) for controlling the steering.
The duty cycle of the PWM signal determines the steering angle, with 0% being full left, 50% being straight, and 100% being full right.
Only one of the LEFT or RIGHT signals should be active at a time to prevent damage to the motor driver.

### Wifi
Uses a captive portal library.
If no saved wifi configuration is found, it can't connect or wrong password for the wifi, it will create an access point with the name "ESP32-Car" and a captive portal to allow the user to connect and configure the wifi settings.
Once the wifi is configured and the ESP32 is connected to the network, it will serve the web interface for controlling the car.

### Web Interface
The web interface is served by the ESP32 and can be accessed by connecting to the same Wi-Fi network and navigating to the ESP32's IP address in a web browser.
The interface includes buttons for left and right and a joystick for forward and backward control. The speed is controlled by the foot pedal, which sends the speed value to the ESP32 via websocket connection.
The web interface is designed to be responsive and can be accessed from both desktop and mobile devices.

### TODO:
- 12v power
- PIN34,35 for current sensing on the motor drivers, show on web interface for debugging (2k2 resistors for voltage divider to step down to 0-3.3v range)
- oscilloscope on driver, see if 255 = 100% duty cycle
- test DRV8871 for steering
- show pedal on web interface for debugging
