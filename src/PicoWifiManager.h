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
        String getHostname() {return creds.hostname;}
        String getSSID() {return creds.ssid;}
        String getIP() {return WiFi.localIP().toString().c_str();}

    private:
        m_states_t m_state {CONNECT};
        bool m_connected {false};   // true if wifi connected and synchronized w/ntp
        HardwareSerial& m_Serial;   // alternate serial output
        int m_retryCount {0};
        uint m_lastTry;             // last time we tried to connect
        static constexpr uint m_minRetryWait {15000};   // minimum wait between retries
        static constexpr uint m_connectWait {3000};
        static constexpr uint m_monitorWait {1000};
        static constexpr int m_maxRetries {10};
        static constexpr uint m_maxNtpWait {60000};
        uint m_waitTimer;
        uint m_ntpStart;
        struct PicoCreds {
            uint32_t signature {};  // write signature
            char hostname[32] {};   // hostname for the pico
            char ssid[64] {};       // wifi ssid
            char psk[80] {};        // wifi psk
        };
        PicoCreds creds;
        void writeCreds();          // write wifi credentials to EEPROM
        bool readCreds();           // read wifi credentials from EEPROM
        static constexpr uint m_eepromSize {256};
        static constexpr uint m_credsAddr {0};      // EEPROM start address for credentials
        static constexpr uint32_t m_haveCreds {0xdeadbeef};
};
