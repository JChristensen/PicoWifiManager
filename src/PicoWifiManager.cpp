// Arduino wifi manager for Raspberry Pi Pico.
// https://github.com/JChristensen/PicoWifiManager
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#include <PicoWifiManager.h>

void PicoWifiManager::begin()
{
    EEPROM.begin(m_eepromSize); // for credentials
    if (!readCreds()) {
        m_Serial.printf("\nWifi credentials not found!\n");
        getCreds();
    }
}

// run the state machine. returns true if connected to wifi.
bool PicoWifiManager::run()
{
    uint ms = millis();

    switch(m_state) {

        // attempt to connect to wifi, reboot if we have tried too many times.
        case CONNECT:
            if (++m_retryCount > m_maxRetries) {
                m_Serial.printf("%d Too many retries.\n", millis());
                resetMCU(10);
            }
            m_state = CONNECT_WAIT;
            m_Serial.printf("%d Connecting to: %s\n", millis(), creds.ssid);
            m_lastTry = millis();
            WiFi.setHostname(creds.hostname);
            WiFi.begin(creds.ssid, creds.psk);
            m_waitTimer = millis();
            break;

        // wait a few seconds for the connection.
        case CONNECT_WAIT:
            if (millis() - m_waitTimer >= m_connectWait) {
                m_state = CONNECT_CHECK;
            }
            break;

        // check to see if we connected successfully.
        // if so, move to monitor mode. if not, try connecting again.
        case CONNECT_CHECK:
            if (WiFi.status() == WL_CONNECTED) {
                m_state = NTP_WAIT;
                m_Serial.printf("%d WiFi connected %s %s %d dBm\n",
                    millis(), WiFi.localIP().toString().c_str(), creds.hostname, WiFi.RSSI());
                m_retryCount = 0;
                m_Serial.printf("%d Starting NTP\n", millis());
                NTP.begin("pool.ntp.org");
                m_waitTimer = millis();
                m_ntpStart = m_waitTimer;
            }
            else {
                m_state = RETRY_WAIT;
            }
            break;

        // this state enforces a minimum wait between connection retries.
        case RETRY_WAIT:
            if (millis() - m_lastTry >= m_minRetryWait) {
                m_state = CONNECT;
            }
            break;

        // wait for response from ntp server
        case NTP_WAIT:
            if (ms - m_ntpStart >= m_maxNtpWait) {
                m_Serial.printf("%d NTP timeout\n", ms);
                resetMCU(10);
            }
            else if (ms - m_waitTimer >= 10) {
                m_waitTimer = ms;
                time_t now = time(nullptr);
                if (now > 8 * 3600 * 2) {
                    m_state = MONITOR;
                    m_connected = true;
                    struct tm tminfo;
                    gmtime_r(&now, &tminfo);
                    char msg[64];
                    strftime(msg, sizeof(msg)/sizeof(msg[0]), "%F %T", &tminfo);
                    m_Serial.printf("%d NTP sync %s UTC\n", millis(), msg);
                }
            }
            break;

        // connection established. check it periodically (but no need to check too fast.)
        case MONITOR:
            if (millis() - m_waitTimer >= m_monitorWait) {
                if (WiFi.status() != WL_CONNECTED) {
                    m_state = RETRY_WAIT;
                    m_lastTry = millis();
                    WiFi.disconnect();
                    m_connected = false;
                    m_Serial.printf("%d WiFi connection lost.\n", millis());
                }
                else {
                    m_waitTimer = millis();
                }
            }
            break;
    }
    return m_connected;
}

// get wifi credentials from the user
void PicoWifiManager::getCreds()
{
    EEPROM.begin(m_eepromSize);
    m_Serial.setTimeout(600000);

    // maybe a little security, start with a clean slate
    char* p = reinterpret_cast<char*>(&creds);
    for (uint i=0; i<sizeof(creds); i++) *(p+i) = '\0';

    m_Serial.printf("\nEnter the wifi SSID: ");
    int nBytes = m_Serial.readBytesUntil('\n', creds.ssid, sizeof(creds.ssid)-1);
    creds.ssid[nBytes] = '\0';

    m_Serial.printf("\nEnter the wifi PSK: ");
    nBytes = m_Serial.readBytesUntil('\n', creds.psk, sizeof(creds.psk)-1);
    creds.psk[nBytes] = '\0';

    m_Serial.printf("\nEnter the hostname for this Pico: ");
    nBytes = m_Serial.readBytesUntil('\n', creds.hostname, sizeof(creds.hostname)-1);
    creds.hostname[nBytes] = '\0';

    creds.signature = m_haveCreds;
    m_Serial << "\nWriting credentials to EEPROM.\n";
    writeCreds();
    resetMCU(3);
}

// write wifi credentials to EEPROM
void PicoWifiManager::writeCreds()
{
    EEPROM.put(m_credsAddr, creds);
    EEPROM.commit();
}

// read wifi credentials from EEPROM
bool PicoWifiManager::readCreds()
{
    EEPROM.get(m_credsAddr, creds);
    return (creds.signature == m_haveCreds);
}

void PicoWifiManager::resetMCU(int seconds)
{
    m_Serial.printf("%d Reboot in ", millis());
    for (int i=seconds; i>=1; i--) {
        m_Serial.printf("%d ", i);
        delay(1000);
    }
    m_Serial.printf("\n\n");
    rp2040.reboot();
}
