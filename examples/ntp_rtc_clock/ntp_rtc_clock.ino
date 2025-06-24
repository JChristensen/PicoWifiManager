// An NTP and RTC clock that can set the RTC time from NTP.
// Press and release the button to switch between display modes.
// Long press to set the RTC time from NTP.
// Wifi credentials are stored in the Pico's EEPROM. If credientials were not saved
// previously, then the sketch will prompt for them.
// Hold the button down while resetting the MCU to change wifi credentials.
// Works with either DS323x or MCP7941x RTC; type detected automatically.
// Time from RTC on i2c1 (Wire1) using 1Hz interrupt.
// Display on SSD1306 OLED on i2c0 (Wire).
// For RPi Pico.
// J.Christensen 13Jun2025

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DS3232RTC.h>          // https://github.com/JChristensen/DS3232RTC
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <MCP79412RTC.h>        // https://github.com/JChristensen/MCP79412RTC
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <Streaming.h>          // https://github.com/janelia-arduino/Streaming
#include <Timezone.h>           // https://github.com/JChristensen/Timezone

// constant parameters
constexpr int wireSdaPin {12}, wireSclPin {13};     // I2C pins for Wire
constexpr int wire1SdaPin {26}, wire1SclPin {27};   // I2C pins for Wire1
constexpr uint8_t ds_addr {0x68}, mcp_addr {0x6f};
constexpr int wifiLED {7};          // illuminates to indicate wifi connected
constexpr int btnPin {14};          // display select/RTC set
constexpr int RTC_1HZ_PIN {22};
constexpr int OLED_WIDTH {128};     // OLED display width, in pixels
constexpr int OLED_HEIGHT {64};     // OLED display height, in pixels
constexpr int OLED_ADDRESS {0x3c};  // OLED I2C address
constexpr uint8_t RTC_SET_ADDR{92}; // MCP7941x SRAM address where the set time is saved

// object instantiations and globals
HardwareSerial& mySerial {Serial};          // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire);
GenericRTC* rtc;
Button btn(btnPin);
bool rtcIsDS{false}, rtcIsMCP{false};

void setup()
{
    pinMode(wifiLED, OUTPUT);
    btn.begin();
    mySerial.begin(115200);
    while (!mySerial && millis() < 2000) delay(50);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);

    // set up and initialize the display
    Wire.setSDA(wireSdaPin); Wire.setSCL(wireSclPin); Wire.setClock(400000);
    Wire.begin();
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        mySerial.printf("SSD1306 allocation failed\n");
        for(;;); // Don't proceed, loop forever
    }
    oled.display();  // library initializes with adafruit logo
    delay(1000);
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    
    // set up and initialize the RTC
    Wire1.setSDA(wire1SdaPin); Wire1.setSCL(wire1SclPin); Wire1.setClock(400000);
    pinMode(RTC_1HZ_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RTC_1HZ_PIN), incrementTime, FALLING);

    // determine which type of RTC we have and perform appropriate setup
    Wire1.begin();
    rtc = findRTC();

    // check to see if the user wants to enter new wifi credentials, else initialize wifi.
    btn.read();
    if (btn.isPressed()) wifi.getCreds();
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Connecting\nto wifi...");
    oled.display();
    wifi.begin();
}

void loop()
{
    static int dispState{0};    // 0=NTP, 1=RTC, 2=BOTH
    static time_t ntpLast{0}, rtcLast{0};
    
    bool wifiStatus = wifi.run();
    digitalWrite(wifiLED, wifiStatus);
    
    if (wifiStatus) {
        btn.read();
        if (btn.wasReleased()) {
            ntpLast = rtcLast = 0;
            if (++dispState > 2) dispState = 0;
        }
        else if (btn.pressedFor(1000)) {
            mySerial << "Setting RTC...\n";
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.println("Setting\nRTC...");
            oled.display();
            while (btn.isPressed()) btn.read();
            setRTC();
        }

        time_t ntpNow = time(nullptr);
        time_t rtcNow = getUTC();
        switch (dispState) {
            case 0:
                if (ntpLast != ntpNow) {
                    ntpLast = ntpNow;
                    displayTime(ntpNow, 'N');
                }
                break;

            case 1:
                if (rtcLast != rtcNow) {
                    rtcLast = rtcNow;
                    displayTime(rtcNow, 'R');
                }
                break;

            case 2:
                if (ntpLast != ntpNow || rtcLast != rtcNow) {
                    ntpLast = ntpNow;
                    rtcLast = rtcNow;
                    displayBoth(ntpNow, rtcNow);
                }
                break;
        }

        // print ntp time and rtc time to serial once a minute
        static time_t printLast{0};
        if (printLast != ntpNow && second(ntpNow) == 0) {
            printLast = ntpNow;
            mySerial.printf("NTP %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC  ",
                year(ntpNow), month(ntpNow), day(ntpNow),
                hour(ntpNow), minute(ntpNow), second(ntpNow));
            mySerial.printf("RTC %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
                year(rtcNow), month(rtcNow), day(rtcNow),
                hour(rtcNow), minute(rtcNow), second(rtcNow));
        }
    }
    else {
        oled.clearDisplay();
        oled.setCursor(0, 0);
        oled.println("Connecting\nto wifi...");
        oled.display();
        delay(50);
    }
}

GenericRTC* findRTC()
{
    Wire1.beginTransmission(ds_addr);
    if (Wire1.endTransmission() == 0) {
        Serial << "Found DS323x RTC\n";
        DS3232RTC* ds_rtc{ new DS3232RTC{Wire1} };
        ds_rtc->begin();
        ds_rtc->squareWave(DS3232RTC::SQWAVE_1_HZ);   // 1 Hz square wave
        rtcIsDS = true;

        time_t utc = getUTC();          // synchronize with RTC
        while ( utc == getUTC() );      // wait for increment to the next second
        utc = ds_rtc->get();            // get the time from the RTC
        setUTC(utc);                    // set our time to the RTC's time
        mySerial << "RTC sync\n";
        return ds_rtc;
    }

    Wire1.beginTransmission(mcp_addr);
    if (Wire1.endTransmission() == 0) {
        Serial << "Found MCP7941x RTC\n";
        MCP79412RTC* mcp_rtc{ new MCP79412RTC{Wire1} };
        mcp_rtc->begin();
        if (!mcp_rtc->isRunning()) mcp_rtc->set(0);        // start the rtc if not running
        mcp_rtc->squareWave(MCP79412RTC::SQWAVE_1_HZ); // 1 Hz square wave

        // print the RTC ID
        // 8-byte RTC "unique ID" with access to upper and lower halves
        union {
            uint8_t b[8];
            struct {
                uint32_t hi;
                uint32_t lo;
            };
        } rtcID;
        mcp_rtc->idRead(rtcID.b);
        mySerial << "RTC ID: ";
        for (int i=0; i<8; i++) mySerial << (rtcID.b[i] < 16 ? "0" : "" ) << _HEX(rtcID.b[i]);
        mySerial << "\n";

        // set the RTC calibration from EEPROM if present, else just print current calibration value
        if (mcp_rtc->eepromRead(125) == 0xAA && mcp_rtc->eepromRead(126) == 0x55) {
            int8_t cal = static_cast<int8_t>(mcp_rtc->eepromRead(127));
            mcp_rtc->calibWrite(cal);   // set calibration register from stored value
            mySerial.printf("RTC calibration value: %d (set from RTC EEPROM)\n", cal);
        }
        else {
            mySerial.printf("RTC calibration value: %d\n", mcp_rtc->calibRead());
        }
    
        // print the time the RTC was last set (from RTC SRAM)
        union {uint8_t b[4]; time_t t;} lastSet;    // union to access a time_t byte by byte
        mcp_rtc->readRTC(RTC_SET_ADDR, lastSet.b, 4);        
        mySerial.printf("RTC was last set at: %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
            year(lastSet.t), month(lastSet.t), day(lastSet.t),
            hour(lastSet.t), minute(lastSet.t), second(lastSet.t));    
    
        time_t utc = getUTC();          // synchronize with RTC
        while ( utc == getUTC() );      // wait for increment to the next second
        utc = mcp_rtc->get();           // get the time from the RTC
        setUTC(utc);                    // set our time to the RTC's time
        mySerial << "RTC sync\n";

        rtcIsMCP = true;
        return mcp_rtc;
    }

    if (!rtcIsDS && !rtcIsMCP) {
        Serial << "Error: No RTC or unknown RTC type.\n";
        while (true);
    }
    return nullptr; // will never get here but we have to keep the compiler happy
}

// set the RTC from current NTP time
void setRTC()
{
    time_t ntpNow = time(nullptr);
    union {uint8_t b[4]; time_t t;} t;      // union to access a time_t byte by byte
    while ((t.t=time(nullptr)) == ntpNow);  // wait for the next second to roll over
    rtc->set(t.t);
    setUTC(t.t);
    if (rtcIsMCP) {
        rtc->writeRTC(RTC_SET_ADDR, t.b, 4);    // //save the utc when time set in sram
    }
    mySerial.printf("RTC set to %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
        year(t.t), month(t.t), day(t.t), hour(t.t), minute(t.t), second(t.t));
}

// update oled display
void displayTime(time_t t, char which)
{
    constexpr TimeChangeRule edt {"EDT", Second, Sun, Mar, 2, -240};  // Daylight time = UTC - 4 hours
    constexpr TimeChangeRule est {"EST", First, Sun, Nov, 2, -300};   // Standard time = UTC - 5 hours
    static Timezone eastern(edt, est);

    constexpr int MSG_SIZE {64};
    static char msg[MSG_SIZE];

    TimeChangeRule* tcr;
    time_t local = eastern.toLocal(t, &tcr);
    struct tm tminfo;
    gmtime_r(&local, &tminfo);

    // update oled display
    oled.clearDisplay();
    strftime(msg, 16, " %T", &tminfo);
    oled.setCursor(0, 0);
    oled.print(which);
    oled.println(msg);
    strftime(msg, 16, "%a %d %b", &tminfo);
    oled.setCursor(0, 20);
    oled.println(msg);
    sprintf(msg, " %d %s", tminfo.tm_year+1900, tcr->abbrev);
    oled.setCursor(0, 42);
    oled.println(msg);
    oled.display();
}

// show ntp time and rtc time on the oled display
void displayBoth(time_t ntp, time_t rtc)
{
    constexpr int MSG_SIZE {64};
    static char msg[MSG_SIZE];
    struct tm tminfo;

    oled.clearDisplay();
    gmtime_r(&ntp, &tminfo);
    strftime(msg, 16, " %T", &tminfo);
    oled.setCursor(0, 0);
    oled.println("UTC times");
    oled.print('N');
    oled.println(msg);

    gmtime_r(&rtc, &tminfo);
    strftime(msg, 16, " %T", &tminfo);
    oled.print('R');
    oled.println(msg);

    sprintf(msg, "RTC %+d s", static_cast<int>(rtc-ntp));
    oled.print(msg);
    oled.display();
}

volatile time_t isrUTC;         // ISR's copy of current RTC time in UTC

// return current RTC time
time_t getUTC()
{
    noInterrupts();
    time_t utc = isrUTC;
    interrupts();
    return utc;
}

// set the current RTC time
void setUTC(time_t utc)
{
    noInterrupts();
    isrUTC = utc;
    interrupts();
}

// 1Hz RTC interrupt handler increments the current time
void incrementTime()
{
    ++isrUTC;
}
