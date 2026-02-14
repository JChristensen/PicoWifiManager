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
            m_Serial.printf("%d Connecting to wifi...\n", millis());
            m_lastTry = millis();
            WiFi.setHostname(creds.hostname);
            //WiFi.begin(creds.ssid, creds.psk);
            multi.run();
            m_waitTimer = millis();
            break;

        // wait a bit for the connection.
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
                m_Serial.printf("%d Connected to %s %s %s %d dBm\n",
                    millis(), WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(), creds.hostname, WiFi.RSSI());
                m_retryCount = 0;
                m_Serial.printf("%d Starting NTP %s", millis(), m_ntp1);
                if (m_ntp2 == nullptr) {
                    m_Serial << endl;
                    NTP.begin(m_ntp1);
                }
                else {
                    m_Serial.printf(" %s\n", m_ntp2);
                    NTP.begin(m_ntp1, m_ntp2);
                }
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
                if (now >= 1735689600) {    // 01Jan2025
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

    m_Serial.printf("\nEnter the hostname for this Pico: ");
    int nBytes = m_Serial.readBytesUntil('\n', creds.hostname, sizeof(creds.hostname)-1);
    creds.hostname[nBytes] = '\0';

    m_Serial.printf("\nEnter the API key: ");
    nBytes = m_Serial.readBytesUntil('\n', creds.apiKey, sizeof(creds.apiKey)-1);
    creds.apiKey[nBytes] = '\0';

    m_Serial.printf("\nEnter MQTT broker hostname: ");
    nBytes = m_Serial.readBytesUntil('\n', creds.mqBroker, sizeof(creds.mqBroker)-1);
    creds.mqBroker[nBytes] = '\0';

    m_Serial.printf("\nEnter MQTT broker port number: ");
    char txtPort[8] {};
    nBytes = m_Serial.readBytesUntil('\n', txtPort, sizeof(txtPort)-1);
    txtPort[nBytes] = '\0';
    creds.mqPort = atoi(txtPort);

    m_Serial.printf("\nEnter MQTT publish topic: ");
    nBytes = m_Serial.readBytesUntil('\n', creds.mqTopic, sizeof(creds.mqTopic)-1);
    creds.mqTopic[nBytes] = '\0';

    int n {0};
    while (n < 4) {
        m_Serial.printf("\nEnter #%d wifi SSID: ", n+1);
        nBytes = m_Serial.readBytesUntil('\n', creds.ssid[n], sizeof(creds.ssid[n])-1);
        creds.ssid[n][nBytes] = '\0';

        m_Serial.printf("\nEnter #%d wifi PSK: ", n+1);
        nBytes = m_Serial.readBytesUntil('\n', creds.psk[n], sizeof(creds.psk[n])-1);
        creds.psk[n][nBytes] = '\0';

        char ans[16] {};
        if (++n < 4) {
            m_Serial.printf("\nEnter another SSID? [y/N]: ");
            nBytes = m_Serial.readBytesUntil('\n', ans, sizeof(ans)-1);
            if (ans[0] != 'Y' && ans[0] != 'y') break;
        }
    }

    creds.ssidCount = n;
    creds.signature = m_haveCreds;
    m_Serial << "\nWriting credentials to EEPROM.\n";
    writeCreds();
}

// write wifi credentials to EEPROM
void PicoWifiManager::writeCreds()
{
    EEPROM.put(m_credsAddr, creds);
    EEPROM.commit();
    readCreds();    // read back to add the APs
}

// read wifi credentials from EEPROM
bool PicoWifiManager::readCreds()
{
    EEPROM.get(m_credsAddr, creds);
    if (creds.signature == m_haveCreds) {
        for (int n=0; n<creds.ssidCount; ++n) {
            multi.addAP(creds.ssid[n], creds.psk[n]);
        }
        return true;
    }
    return false;
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
