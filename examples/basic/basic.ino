// A basic sketch that does nothing other than connect to wifi and print
// the time every minute. Output is on Serial2 (aka UART-1).
// For Raspbery Pi Pico W or 2W.
// Pin assignments for Pico Carrier Board v1.0.
// J.Christensen 14Nov2025

#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time

// constant parameters
constexpr int txPin {4}, rxPin {5};
constexpr int wifiLED {7};          // illuminates to indicate wifi connected

// object instantiations and globals
HardwareSerial& mySerial {Serial2}; // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);

void setup()
{
    pinMode(wifiLED, OUTPUT);
    Serial2.setTX(txPin); Serial2.setRX(rxPin);
    mySerial.begin(115200); delay(500);
    while (!mySerial && millis() < 2000) delay(50);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);

    wifi.begin();
}

void loop()
{
    bool wifiStatus = wifi.run();
    digitalWrite(wifiLED, wifiStatus);

    if (wifiStatus) {
        time_t ntpNow = time(nullptr);
        // print ntp time to serial once a minute
        static time_t printLast{0};
        if (printLast != ntpNow && second(ntpNow) == 0) {
            printLast = ntpNow;
            mySerial.printf("%d %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
                millis(), year(ntpNow), month(ntpNow), day(ntpNow),
                hour(ntpNow), minute(ntpNow), second(ntpNow));
        }
    }
    else {
        delay(50);
    }
}
