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

void loop()
{
    if (state == STATE::NONE)
    {
        state = STATE::GETTING_INTAKE_AIR_TEMP;
    }

    if (state == STATE::GETTING_INTAKE_AIR_TEMP)
    {
        float val = myELM327.intakeAirTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
        {
            data.intakeAirTemp = (uint32_t)val;
            Serial.println("intakeAirTemp:" + String(data.intakeAirTemp) + ",");
            state = STATE::GETTING_ENGINE_COOLANT_TEMP;
        }
        else if ((myELM327.nb_rx_state == ELM_NO_DATA))
            val = myELM327.intakeAirTemp();
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
            myELM327.printError();
    }

    if (state == STATE::GETTING_ENGINE_COOLANT_TEMP)
    {
        float val = myELM327.engineCoolantTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
        {
            data.engineCoolantTemp = (uint32_t)val;
            Serial.println("engineCoolantTemp:" + String(data.engineCoolantTemp) + ",");
            state = STATE::GETTING_RPM;
        }
        else if ((myELM327.nb_rx_state == ELM_NO_DATA))
            val = myELM327.engineCoolantTemp();
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
            myELM327.printError();
    }

    if (state == STATE::GETTING_RPM)
    {
        float val = myELM327.rpm();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
        {
            data.rpm = (uint32_t)val;
            Serial.println("RPM:" + String(data.rpm) + ",");
            state = STATE::GETTING_MPH;
        }
        else if ((myELM327.nb_rx_state == ELM_NO_DATA))
            val = myELM327.rpm();
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
            myELM327.printError();
    }

    if (state == STATE::GETTING_MPH)
    {
        float val = myELM327.mph();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
        {
            data.mph = (uint32_t)val;
            Serial.println("mph:" + String(data.mph) + ",");
            state = STATE::NONE;
        }
        else if ((myELM327.nb_rx_state == ELM_NO_DATA))
            val = myELM327.mph();
        else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
            myELM327.printError();
    }

    // if (lastChangedAt == 0 || millis() - lastChangedAt > 5000)
    // {
    //     lastChangedAt = millis();
    //     data.engineCoolantTemp = random(85, 120);
    //     data.intakeAirTemp = random(45, 72);
    //     data.rpm = random(830, 4550);
    //     data.mph = random(0, 156);
    // }

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