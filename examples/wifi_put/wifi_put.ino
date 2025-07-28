// A basic sketch to periodically send data from the Pico's internal temperature
// sensor to GroveStreams via HTTP PUT.
// This is not very interesting data, but it does demonstrate the technique.
// Debug output is on Serial2 (aka UART-1).
// For Raspbery Pi Pico W or 2W.
// J.Christensen 28Jul2025

#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <TimeLib.h>
#include <WiFi.h>

// constant parameters
constexpr uint32_t txInterval {10}; // how often to send data, minutes
constexpr uint32_t retryInterval {60}, respTimeout {60};    // seconds
const char* host {"grovestreams.com"};
constexpr uint16_t port {80};
const char* apiKey;             // grovestreams api key, read from eeprom
const char* compID {"pico"};    // grovestreams component ID
constexpr int txPin {4}, rxPin {5};
constexpr int waitLED {7};      // turns on while waiting for the server to respond
constexpr int btnPin {14};      // button to force prompt for wifi credentials

// object instantiations and globals
HardwareSerial& mySerial {Serial2}; // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
Button btn(btnPin);
time_t nextSend {};             // next time to transmit data

void setup()
{
    pinMode(waitLED, OUTPUT);
    btn.begin();
    Serial2.setRX(rxPin); Serial2.setTX(txPin);
    mySerial.begin(115200);
    while (!mySerial && millis() < 2000) delay(50); delay(500);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);

    // check to see if the user wants to enter new wifi credentials, else initialize wifi.
    btn.read();
    if (btn.isPressed()) wifi.getCreds();
    wifi.begin();
    while (!wifi.run());    // wait for wifi and ntp to start
    calcNextSend();         // calculate first transmit time
    apiKey = wifi.getApiKey();
    mySerial.printf("%d First send will be %.4d-%.2d-%.2d %.2d:%.2d:%.2d\n",
        millis(), year(nextSend), month(nextSend), day(nextSend), hour(nextSend), minute(nextSend), second(nextSend));
}

void loop()
{
    enum states_t {CONNECT, WAIT_RETRY, WAIT_RECV, PROCESS_RESPONSE, WAIT_CONNECT};
    static states_t state {WAIT_CONNECT};
    static WiFiClient client;
    static uint32_t msTimer;
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
                        "PUT /api/feed?&api_key=%s&compID=%s"
                        "&temp=%.2f"
                        " HTTP/1.1\r\nHost: %s\r\n"
                        "Connection: close\r\nX-Forwarded-For: %s\r\n"
                        "Content-Type: application/json\r\n\r\n",
                        apiKey, compID, analogReadTemp(), host, compID);
                    msTimer = msNow;
                }
                else {
                    state = WAIT_RETRY;
                    msTimer = msNow;
                    digitalWrite(waitLED, LOW);
                    mySerial.printf("%d Connect failed, wait for retry...\n", msNow);
                }
                break;

            case WAIT_RETRY:
                if (msNow - msTimer >= retryInterval * 1000) state = CONNECT;
                break;

            case WAIT_RECV:
                if (client.available() > 0) {
                    state = PROCESS_RESPONSE;
                    digitalWrite(waitLED, LOW);
                    mySerial.printf("%d Server responded: ", msNow);
                }
                else if (msNow - msTimer >= respTimeout * 1000) {
                    client.stop();
                    digitalWrite(waitLED, LOW);
                    state = CONNECT;
                    mySerial.printf("\n%d Response timeout!\n", msNow);
                }
                break;

            case PROCESS_RESPONSE: {
                uint nbytes = client.available();       // number of characters in received packet
                const char http200[] {"HTTP/1.1 200 "}; // good status
                if (nbytes >= sizeof(http200)) {
                    // collect enough characters to check the status
                    char statusBuf[16];     // buffer to store the first header line
                    char* p {statusBuf};
                    for (uint i=0; i<sizeof(http200)-1; i++) *p++ = client.read();
                    *p++ = '\0';
                    if (strncmp(statusBuf, http200, sizeof(http200)-1) == 0) {
                        mySerial.printf("%s(OK)\n", statusBuf);
                        // good response, drop the rest
                        while (client.available()) {
                            client.read();
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
                calcNextSend();
                break;
            }

            case WAIT_CONNECT:
                if (time(nullptr) >= nextSend) state = CONNECT;
                break;
        }
    }
}

// calculate the next time to transmit data
void calcNextSend()
{
    time_t t = time(nullptr);
    nextSend = t - t % (txInterval * 60) + txInterval * 60;
    //mySerial.printf("%d Next send %.4d-%.2d-%.2d %.2d:%.2d:%.2d\n",
    //    millis(), year(nextSend), month(nextSend), day(nextSend), hour(nextSend), minute(nextSend), second(nextSend));
    return;
}
