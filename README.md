# ESP32_Socket_Server
Basic Socket Server


Note: which is the default open-source TCP/IP stack embedded within Espressif's official development framework, ESP-IDF.lwIP fully supports the industry-standard POSIX / BSD Sockets API. This means you do not have to write a network stack from scratch, but you will be building your clients and servers using the raw fundamental building blocks of internet

# Set up

Install ESP-IDF: Download and install the official Espressif IoT Development Framework (v5.x recommended) for your OS.

Configure VS Code: Install the ESP-IDF extension in Visual Studio Code to make flashing and monitoring your device easy.

Test Blink: Flash a simple C program to blink an onboard LED using gpio_set_level() to ensure your computer can talk to the chip.

# Local Reciever 

Write a Python Receiver: Create a simple 20-line Python script on your PC using the native socket library.

Bind to a Port: Set the script to bind() to your computer's local IP address on an open port (like 8080) and execute listen().

Print Inbound Data: Ensure the script accepts a connection and loops recv() to print any incoming string to your PC terminal.

# WiFi Provisioning (The Link Layer)

Import FreeRTOS & Wi-Fi Components: Include <esp_wifi.h> and <esp_event.h> in your C project.

Initialize the Wi-Fi Stack: Write the boilerplate code to initialize the underlying network interface (esp_netif_init()).

Connect as a Station: Hardcode your home Wi-Fi SSID and password using the WIFI_MODE_STA configuration profile.

Verify IP Assignment: Use event handlers to detect a successful connection and print the assigned IPv4 address to the serial log.

#  Sockets & Data Streaming (Transport & Application Layers)

Create the Socket: Use the POSIX socket(AF_INET, SOCK_STREAM, 0) function to request a TCP socket.

Configure Destination: Populate a sockaddr_in struct with your PC’s local IP address and port 8080.

Execute the Handshake: Call connect() to trigger the hardware-level TCP 3-way handshake with your PC.

Stream Payloads: Create a loop that packages a dummy payload (like an incrementing loop counter string) and transmits it using send().

# Verification & Hardening (The "Engineer" Phase)

Wireshark Analysis: Run Wireshark on your PC, filter traffic by ip.addr == [ESP32_IP], and visually inspect the TCP packets moving between devices.

Handle Network Failure: Add try/catch style error checking to your C code. If send() returns a negative value (broken pipe), force the ESP32 to close() the socket, wait 5 seconds, and try to reconnect.

Upgrade to Physical Sensors: Replace the dummy counter data with real sensor data (like a DHT11 temperature sensor) read via I2C or GPIO.


# End Goal

Wireless Autonomy: The ESP32 boots up, initializes its Wi-Fi radio using the ESP-IDF framework, logs into your home router, and successfully prints its dynamically assigned local IP address to the terminal.

Reliable Data Stream: The ESP32 establishes a stable TCP connection to your computer and continuously streams live data (like a timestamp, a counter, or sensor readouts) without crashing or leaking memory.

A Custom Receiver Application: You write a secondary, lightweight program on your computer (in Python or C) that acts as the target server. It sits listening on a specific port, accepts the ESP32’s connection, reads the incoming byte buffer, and displays the information.

Packet-Level Verification: You fire up Wireshark on your computer, apply a filter for your ESP32's IP address, and capture the exact packet structure, visually verifying the TCP handshakes and data payloads you engineered.

Error Resilience: You test the system's toughness by turning off your router or turning off the computer server. Your ESP32 C code should gracefully detect the lost connection and enter a loop to retry without bricking the device.


[ ESP32 Microcontroller ]                 [ Your Local Router ]                 [ Your Personal Computer ]
   (Reads Sensor via C)                     (Manages Network)                     (Runs Python/C Backend)
            │                                       │                                       │
            ├───[ Connects via Wi-Fi Station ]─────>│                                       │
            │                                       ├───[ Forwards Packets via TCP/IP ]────>│
            │                                       │                                       │
            └───[ Sends Raw Bytes via Socket ]──────┼──────────────────────────────────────>│ (Accepts Connection)
                                                                                            | (Saves or Displays Data)
