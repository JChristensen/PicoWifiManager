// Adapted from the ReadyMail > Sending > SimpleText example sketch.
// J.Christensen 03Dec2025

#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ReadyMail expects these defines
#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial2
#define READYMAIL_TIME_SOURCE time(nullptr);
#include <ReadyMail.h>          // https://github.com/mobizt/ReadyMail

const String SMTP_HOST {"smtp.mail.yahoo.com"};
constexpr int SMTP_PORT {465};  // 465 for SSL, or 587 for STARTTLS

// email addresses *in the message headers* can be of one of these forms:
// "Some Name <some.email@foo.com>" or "<some.email@foo.com>" or "some.email@foo.com"
// *for authentication*, i.e. smtp.authenticate(), use the email address only, e.g. my.email@foo.com
const String AUTHOR_EMAIL {"your.email@foo.com"};
const String AUTHOR_NAME {"Ford Prefect"};
const String AUTHOR_PASSWORD {"________"};  // this is an "app password", not the logon password.
const String RECIPIENT_EMAIL {"Arthur Dent <arthur.dent@foo.com>"};

constexpr bool SSL_MODE {true};
constexpr bool AUTHENTICATION {true};
const String NOTIFY {"SUCCESS,FAILURE,DELAY"};

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
constexpr int txPin {4}, rxPin {5};     // serial pins
HardwareSerial& mySerial {Serial2};
PicoWifiManager wifi(mySerial);

// status callback. this prints status for the various steps as the mail is sent.
void smtpStatusCB(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void setup()
{
    Serial2.setRX(rxPin); Serial2.setTX(txPin);
    mySerial.begin(115200); delay(500);
    while (!mySerial && millis() < 2000) delay(10);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);
    mySerial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    wifi.begin();
    while (!wifi.run());    // wait for wifi and ntp to start

    ssl_client.setInsecure();
    smtp.connect(SMTP_HOST, SMTP_PORT, smtpStatusCB, SSL_MODE);
    if (!smtp.isConnected())
        return;
    if (AUTHENTICATION) {
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
        if (!smtp.isAuthenticated())
            return;
    }

    SMTPMessage msg;
    msg.headers.add(rfc822_subject, "Pico email test");

    // Multiple recipents can be added, but only one sender and one from.
    msg.headers.add(rfc822_from, AUTHOR_NAME + " <" + AUTHOR_EMAIL + ">");
    msg.headers.add(rfc822_to, RECIPIENT_EMAIL);

    String bodyText = "Hello world,\r\n\r\n";
    bodyText += "This email was sent from a Raspberry Pi Pico W microcontroller, ";
    bodyText += "with the help of the ReadyMail library:\r\n";
    bodyText += "\r\n";
    bodyText += "https://github.com/mobizt/ReadyMail\r\n";
    bodyText += "\r\n";
    bodyText += "Cheers!\r\n";
    bodyText += "\r\n-- \r\n";
    bodyText += "Yours truly,\r\n";
    bodyText += "Sent from a Raspberry Pi Pico W\r\n";

    msg.text.body(bodyText);        // add the body content
    msg.timestamp = time(nullptr);  // set message timestamp
    smtp.send(msg, NOTIFY);
    smtp.stop();    // stop the server connection and release the allocated resources.
}

void loop()
{
    wifi.run();
}
