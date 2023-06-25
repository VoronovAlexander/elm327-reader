#include "BluetoothSerial.h"
#include "ELMduino.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/**
 * Порог срабатывания для мигания
 */
#define INTAKE_AIR_TEMP_THRESHOLD 60
#define ENGINE_COOLANT_TEMP_THRESHOLD 101

/// --- Хар-ки дисплея --- ///

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

/// --- Bluetooth конфигурация --- ///
BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
#define DEBUG_PORT Serial
String name = "OBDII";
String MACadd = "23:32:17:05:88:45";
uint8_t address[6] = {0x17, 0x20, 0x11, 0x05, 0x58, 0x2D};

ELM327 myELM327;

/// --- Data --- ///
struct Data
{
    int intakeAirTemp = 0;
    int engineCoolantTemp = 0;
    int rpm = 0;
    int mph = 0;
};

/// --- Текущее состояние опроса OBD --- ///
enum STATE
{
    NONE,
    GETTING_INTAKE_AIR_TEMP,
    GETTING_ENGINE_COOLANT_TEMP,
    GETTING_RPM,
    GETTING_MPH,
};

/**
 * Выводит текст на дисплей
 */
void printText(int x, int y, int size, uint16_t color, String text)
{
    display.setTextColor(color);

    display.setTextSize(size);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

    display.setCursor(x - w / 2, y);
    display.print(text);
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(23, 19);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    display.clearDisplay();
    printText(64, 32, 1, WHITE, "Connecting to OBD scanner - Phase 1");
    display.display();

    // delay(2000);
    // return;

    ELM_PORT.begin("OBDII", true);
    ELM_PORT.setPin("1234");

    Serial.println("Connecting...");
    if (!ELM_PORT.connect(name))
    {
        Serial.println("Couldn't connect to OBD scanner - Phase 1");

        display.clearDisplay();
        printText(64, 32, 1, WHITE, "Couldn't connect to OBD scanner - Phase 1");
        display.display();
        delay(5000);

        ESP.restart();
    }

    display.clearDisplay();
    printText(64, 32, 1, WHITE, "Connecting to OBD scanner - Phase 2");
    display.display();

    if (!myELM327.begin(ELM_PORT, false, 3000))
    {
        Serial.println("Couldn't connect to OBD scanner - Phase 2");

        display.clearDisplay();
        printText(64, 32, 1, WHITE, "Couldn't connect to OBD scanner - Phase 2");
        display.display();
        delay(5000);

        ESP.restart();
    }

    Serial.println("Connected to ELM327");
}

STATE state = STATE::NONE;
Data data;

unsigned lastChangedAt = 0;

int StrToHex(char str[])
{
    return (int)strtol(str, 0, 16);
}

void loop()
{
    // const uint8_t INTAKE_AIR_TEMP = 15;    // 0x0F - °C
    // const uint8_t ENGINE_COOLANT_TEMP = 5; // 0x05 - °C
    // const uint8_t ENGINE_RPM = 12;         // 0x0C - rpm
    // const uint8_t VEHICLE_SPEED = 13;      // 0x0D - km/h

    const char *const CMD = "01 0F 05 0C 0D";

    myELM327.sendCommand_Blocking(CMD);
    String response = myELM327.payload;
    // Response (ON: 01 0F 05 0C 0D): 00A0:410F5F057E0C1:0B120D00AAAAAA
    //             IAT     COOL      RPM        KMH
    // 00A0:410F   5F 05   7E 0C 1 : 0B 12 0D    00 AAAA AA
    // IAT: 5F = 95 - 40 = 55C
    // Coolant: 7E = 126 - 40 = 86C
    // RPM: 0B = 11, 12 = 18 === ((11*256)+18)/4 = 708.5
    // KMH:
    // String response = "00A0:410F5F057E0C1:0B120D00AAAAAA";
    Serial.print("response: ");
    Serial.println(response);

    Serial.print("IAT: ");
    char hexIAT[3] = {response.charAt(9), response.charAt(10), '\0'};
    data.intakeAirTemp = StrToHex(hexIAT) - 40;
    Serial.println(data.intakeAirTemp);

    Serial.print("Coolant: ");
    char hexEngineCoolantTemp[3] = {response.charAt(13), response.charAt(14), '\0'};
    data.engineCoolantTemp = StrToHex(hexEngineCoolantTemp) - 40;
    Serial.println(data.engineCoolantTemp);

    Serial.print("RPM: ");
    char hexRPM_A[3] = {response.charAt(19), response.charAt(20), '\0'};
    char hexRPM_B[3] = {response.charAt(21), response.charAt(22), '\0'};
    data.rpm = ((StrToHex(hexRPM_A) * 256) + StrToHex(hexRPM_B)) / 4;
    Serial.println(data.rpm);

    Serial.print("Mp/h: ");
    char hexKmh[3] = {response.charAt(25), response.charAt(26), '\0'};
    data.mph = (double)StrToHex(hexKmh) / 1.609344;
    Serial.println(data.mph);


    display.clearDisplay();

    uint16_t iatTextColor = WHITE;
    if (data.intakeAirTemp >= INTAKE_AIR_TEMP_THRESHOLD)
    {
        if ((millis() / 300) % 2 == 1)
        {
            iatTextColor = BLACK;
            display.fillRect(0, 0, 64, 32, WHITE);
        }
    }

    uint16_t engineCoolantTempTextColor = WHITE;
    if (data.engineCoolantTemp >= ENGINE_COOLANT_TEMP_THRESHOLD)
    {
        if ((millis() / 300) % 2 == 1)
        {
            engineCoolantTempTextColor = BLACK;
            display.fillRect(0, 32, 64, 32, WHITE);
        }
    }

    // Column 1
    printText(SCREEN_WIDTH / 4 * 1, 0, 1, iatTextColor, "IAT");
    printText(SCREEN_WIDTH / 4 * 1, 10, 2, iatTextColor, String(data.intakeAirTemp));
    printText(SCREEN_WIDTH / 4 * 1, 38, 1, engineCoolantTempTextColor, "COOLANT");
    printText(SCREEN_WIDTH / 4 * 1, 48, 2, engineCoolantTempTextColor, String(data.engineCoolantTemp));

    // Column 2
    printText(SCREEN_WIDTH / 4 * 3, 0, 1, WHITE, "RPM");
    printText(SCREEN_WIDTH / 4 * 3, 10, 2, WHITE, String(data.rpm));
    printText(SCREEN_WIDTH / 4 * 3, 38, 1, WHITE, "KM/H");
    int kmh = ((double)data.mph * 1.609344);
    printText(SCREEN_WIDTH / 4 * 3, 48, 2, WHITE, String(kmh));

    display.drawLine(0, 32, 128, 32, WHITE);
    display.drawLine(64, 0, 64, 64, WHITE);

    display.display();
}