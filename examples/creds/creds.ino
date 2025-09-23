// Sketch to read and store hostname and wifi credentials in RPi Pico EEPROM.
// J.Christensen 28Jul2025
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <Streaming.h>          // https://github.com/janelia-arduino/Streaming
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager

constexpr uint btnPin {14};

// object instantiations and globals
HardwareSerial& mySerial {Serial2};  // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
Button btn(btnPin);

void setup()
{
    btn.begin();
    Serial2.setTX(4); Serial2.setRX(5);
    mySerial.begin(115200); delay(500);
    while (!mySerial && millis() < 2000) delay(50); delay(250);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);
    wifi.begin();
    mySerial << "Press and release the button to read EEPROM.\n";
    mySerial << "Press and hold the button to write.\n";
}

void loop()
{
    btn.read();
    if (btn.pressedFor(1000)) {
        wifi.getCreds();
        while (btn.isPressed()) btn.read();
        mySerial.printf("\nReading...\n");
        mySerial.printf("hostname %s\nAPI key %s\n",
            wifi.getHostname(), wifi.getApiKey());
        for (int n=0; n<wifi.getSSIDCount(); ++n) {
            mySerial.printf("ssid %s\npsk %s\n",
                wifi.getSSID(n), wifi.getPSK(n));
        }
    }
    else if (btn.wasReleased()) {
        mySerial.printf("\nReading...\n");
        mySerial.printf("hostname %s\nAPI key %s\n",
            wifi.getHostname(), wifi.getApiKey());
        for (int n=0; n<wifi.getSSIDCount(); ++n) {
            mySerial.printf("ssid %s\npsk %s\n",
                wifi.getSSID(n), wifi.getPSK(n));
        }
    }
}
