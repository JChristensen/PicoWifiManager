// An NTP and clock for Raspberry Pi Pico W/2W.
// Wifi credentials are stored in EEPROM. If credientials were not saved
// previously, then the sketch will prompt for them.
// Hold the button down while resetting the MCU to change wifi credentials.
// Display on SSD1306 OLED on i2c0 (Wire).
// J.Christensen 21Jun2025

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <Streaming.h>          // https://github.com/janelia-arduino/Streaming
#include <Timezone.h>           // https://github.com/JChristensen/Timezone

// constant parameters
constexpr int wireSdaPin {12}, wireSclPin {13};     // I2C pins for Wire
constexpr int wifiLED {7};          // illuminates to indicate wifi connected
constexpr int btnPin {14};          // force prompt for wifi credentials
constexpr int OLED_WIDTH {128};     // OLED display width, in pixels
constexpr int OLED_HEIGHT {64};     // OLED display height, in pixels
constexpr int OLED_ADDRESS {0x3c};  // OLED I2C address

// object instantiations and globals
HardwareSerial& mySerial {Serial};  // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire);
Button btn(btnPin);

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
    static time_t ntpLast{0};
    
    bool wifiStatus = wifi.run();
    digitalWrite(wifiLED, wifiStatus);
    
    if (wifiStatus) {
        time_t ntpNow = time(nullptr);
        if (ntpLast != ntpNow) {
            ntpLast = ntpNow;
            displayTime(ntpNow);
        }

        // print ntp time to serial once a minute
        static time_t printLast{0};
        if (printLast != ntpNow && second(ntpNow) == 0) {
            printLast = ntpNow;
            mySerial.printf("NTP %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
                year(ntpNow), month(ntpNow), day(ntpNow),
                hour(ntpNow), minute(ntpNow), second(ntpNow));
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

// update oled display
void displayTime(time_t t)
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
    oled.println(msg);
    strftime(msg, 16, "%a %d %b", &tminfo);
    oled.setCursor(0, 20);
    oled.println(msg);
    sprintf(msg, " %d %s", tminfo.tm_year+1900, tcr->abbrev);
    oled.setCursor(0, 42);
    oled.println(msg);
    oled.display();
}
