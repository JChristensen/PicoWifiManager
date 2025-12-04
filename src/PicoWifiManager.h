// Arduino wifi manager for Raspberry Pi Pico.
// https://github.com/JChristensen/PicoWifiManager
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#pragma once
#include <EEPROM.h>
#include <WiFi.h>
#include <Streaming.h>      // https://github.com/janelia-arduino/Streaming

class PicoWifiManager
{
    enum m_states_t {CONNECT, CONNECT_WAIT, CONNECT_CHECK, RETRY_WAIT, NTP_WAIT, MONITOR};
    public:
        PicoWifiManager(HardwareSerial& hws=Serial)
            : m_Serial{hws} {}
        void begin();
        bool run();
        void getCreds();            // get wifi credentials from the user
        void resetMCU(int seconds);
        char* getHostname() {return creds.hostname;}
        char* getApiKey() {return creds.apiKey;}
        int getSSIDCount() {return creds.ssidCount;}
        char* getSSID(int n) {return creds.ssid[n];}
        char* getPSK(int n) {return creds.psk[n];}
        //probably do not need getIP(), caller can get it directly.
        //String getIP() {return WiFi.localIP().toString().c_str();}
        char* getMqBroker() {return creds.mqBroker;}
        int getMqPort() {return creds.mqPort;}
        char* getMqTopic() {return creds.mqTopic;}

    private:
        m_states_t m_state {CONNECT};
        bool m_connected {false};   // true if wifi connected and synchronized w/ntp
        HardwareSerial& m_Serial;   // alternate serial output
        int m_retryCount {0};
        uint m_lastTry;             // last time we tried to connect
        static constexpr uint m_minRetryWait {15000};   // minimum wait between retries
        static constexpr uint m_connectWait {1000};
        static constexpr uint m_monitorWait {1000};
        static constexpr int m_maxRetries {10};
        static constexpr uint m_maxNtpWait {60000};
        uint m_waitTimer;
        uint m_ntpStart;
        struct PicoCreds {
            char hostname[16] {};   // hostname for the pico
            uint signature {};      // write signature
            char apiKey[40] {};     // api key, e.g. for GroveStreams
            char mqBroker[16] {};   // mqtt broker hostname
            uint mqPort {};         // mqtt port number
            char mqTopic[16] {};    // mqtt topic to publish to
            int ssidCount {};       // number of ssid/psk pairs
            char ssid[4][32] {};    // wifi ssids
            char psk[4][64] {};     // wifi psks
        };
        PicoCreds creds;
        void writeCreds();          // write wifi credentials to EEPROM
        bool readCreds();           // read wifi credentials from EEPROM
        static constexpr uint m_eepromSize {1024};
        static constexpr uint m_credsAddr {0};  // EEPROM start address for credentials
        static constexpr uint32_t m_haveCreds {0xdeaddead};
        WiFiMulti multi;
};
