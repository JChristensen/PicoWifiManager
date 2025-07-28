// A very basic sketch to send an HTTP GET request for a web page.
// Output is on Serial2 (aka UART-1).
// For Raspbery Pi Pico W or 2W.
// J.Christensen 24Jul2025

#include <WiFi.h>
#include <PicoWifiManager.h>        // https://github.com/JChristensen/PicoWifiManager
#include <TimeLib.h>

// object instantiations and globals
HardwareSerial& mySerial {Serial2}; // choose Serial, Serial1 or Serial2 here
constexpr int txPin {4}, rxPin {5};
constexpr uint32_t getInterval {600000}, retryInterval {60000}, respTimeout {60000};
const char* host = "wttr.in";
constexpr uint16_t port = 80;
constexpr int waitLED {7};
PicoWifiManager wifi(mySerial);

void setup()
{
    Serial2.setRX(rxPin); Serial2.setTX(txPin);
    mySerial.begin(115200);
    while (!mySerial && millis() < 2000) delay(50);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU / 1000000);
    pinMode(waitLED, OUTPUT);
    wifi.begin();
}

void loop()
{
    enum states_t {CONNECT, WAIT_RETRY, WAIT_RECV, PROCESS_RESPONSE, WAIT_CONNECT};
    static states_t state {CONNECT};
    static WiFiClient client;
    static uint32_t msTimer, msLastConnect;
    uint32_t msNow = millis();

    if (wifi.run()) {
        switch (state) {
            case CONNECT:
                if (client.connect(host, port)) {
                    state = WAIT_RECV;
                    time_t t = time(nullptr);
                    mySerial.printf("\n%d %.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
                                    msNow, year(t), month(t), day(t), hour(t), minute(t), second(t));
                    mySerial.printf("Sending request to %s ...\n", host);
                    digitalWrite(waitLED, HIGH);
                    client.printf(
                        "GET / HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "User-Agent: curl/8.5.0\r\n"
                        "Accept: */*\r\n\r\n", host);
                    msTimer = msNow;
                    msLastConnect = msNow;
                }
                else {
                    state = WAIT_RETRY;
                    msTimer = msNow;
                    digitalWrite(waitLED, LOW);
                    mySerial.printf("%d Connect failed, wait for retry...\n", msNow);
                }
                break;

            case WAIT_RETRY:
                if (msNow - msTimer >= retryInterval) state = CONNECT;
                break;

            case WAIT_RECV:
                if (client.available() > 0) {
                    state = PROCESS_RESPONSE;
                    digitalWrite(waitLED, LOW);
                    mySerial.printf("%d Receiving from server...\n", msNow);
                }
                else if (msNow - msTimer >= respTimeout) {
                    client.stop();
                    digitalWrite(waitLED, LOW);
                    state = CONNECT;
                    mySerial.printf("%d Response timeout!\n", msNow);
                }
                break;

            case PROCESS_RESPONSE: {
                uint nbytes = client.available();   // number of characters in received packet
                const char http200[14] {"HTTP/1.1 200 "};   // good status
                if (nbytes >= sizeof(http200)) {
                    // collect enough characters to check the status
                    char statusBuf[14];     // buffer to store the first header line
                    char* p {statusBuf};
                    for (uint i=0; i<sizeof(http200); i++) *p++ = client.read();
                    if (strncmp(statusBuf, http200, sizeof(http200)-1) == 0) {
                        // good response, drop the headers, i.e. everything up to and including \r\n\r\n
                        while (client.available()) {
                            if (client.read() == '\r') {
                                if (client.read() == '\n') {
                                    if (client.read() == '\r') {
                                        if (client.read() == '\n') {
                                            while (client.available()) {
                                                mySerial.print(static_cast<char>(client.read()));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else {
                        mySerial.print("HTTP error!\n");
                        mySerial.print(statusBuf);
                        while (client.available()) {
                            char ch = static_cast<char>(client.read());
                            mySerial.print(ch);
                        }
                    }
                }
                else {
                    mySerial.print("Error, packet too short!\n");
                    while (client.available()) {
                        char ch = static_cast<char>(client.read());
                        mySerial.print(ch);
                    }
                }

                state = WAIT_CONNECT;
                mySerial.printf("%d Closing connection.\n", msNow);
                client.stop();
                break;
            }

            case WAIT_CONNECT:
                if (msNow - msLastConnect >= getInterval) state = CONNECT;
                break;
        }
    }
}
