#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSgp40.h>
#include <DFRobot_SHTC3.h>
#include "Zigbee.h"

// PINY
#define PIN_GROVE_BUTTON    D6    
#define PIN_GROVE_LED       D7    
#define LED_PIN             LED_BUILTIN 
#define PIN_FLAME_ANALOG    A0    
#define PIN_FLAME_DIGITAL   D1

// ENDPOINTY
ZigbeeIlluminanceSensor zbFlameRaw  = ZigbeeIlluminanceSensor(1);
ZigbeeIlluminanceSensor zbSgpRaw    = ZigbeeIlluminanceSensor(2);
ZigbeeBinary zbFlameStat            = ZigbeeBinary(3);
ZigbeeBinary zbSgpStat              = ZigbeeBinary(4);
// Zmiana na Analog Output - najlżejszy i najstabilniejszy suwak
ZigbeeAnalog zbSgpThreshold         = ZigbeeAnalog(5);
ZigbeeTempSensor zbTempHum          = ZigbeeTempSensor(6);

SensirionI2CSgp40 sgp40;
DFRobot_SHTC3 shtc3(&Wire);

// ZMIENNE
uint32_t lastMeasureTime = 0;
uint32_t lastBlinkTime = 0;
uint32_t ledTimeOut = 0;
bool lastBtnState = false; 
float last_temp = -99.0;
float last_hum = -99.0;

bool isPairingMode = false;
volatile uint16_t dynamicSgpThreshold = 20000;

uint16_t last_srawVoc = 0;
int last_fAna = 0;
bool last_fDig = false;

uint32_t lastZigbeeRaport = 0;
uint32_t lastZigbeeAlive = 0;
bool zigbeeRaportNow = false;

// LEDY
void setLeds(bool state) {
    digitalWrite(LED_PIN, state ? LOW : HIGH);      
    digitalWrite(PIN_GROVE_LED, state ? HIGH : LOW); 
}

void LED_Raport(uint32_t blinkTime = 500) {
    setLeds(true);
    ledTimeOut = millis() + blinkTime;             
}

void LED_Raport_Clear() {
    if (ledTimeOut != 0 && millis() >= ledTimeOut) {
        setLeds(false);
        ledTimeOut = 0;
    }
}

// CALLBACK DLA SUWAKA (EP5)
void myThresholdCallback(float newValue) {
    // newValue przychodzi bezpośrednio jako liczba z suwaka Z2M
    dynamicSgpThreshold = (uint16_t)newValue;
    zigbeeRaportNow = true; 
    Serial.printf(">>> ZIGBEE: Odebrano nowy prog: %d\n", dynamicSgpThreshold);
}

void setup() {
    setCpuFrequencyMhz(80); 
    Serial.begin(115200);
    delay(2000); 

    Serial.println("--- START: XIAO-C6-StoveSmoke ---");
    
    // Start I2C (D4+D5)
    Wire.begin(D4, D5);
        // SGP40 @I2C
        sgp40.begin(Wire);
        // SHTC3 @I2C
        shtc3.begin(); 
        delay(100); // Mały margines na stabilizację napięcia
        shtc3.wakeup();

    pinMode(PIN_GROVE_BUTTON, INPUT_PULLUP); 
    pinMode(PIN_GROVE_LED, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(PIN_FLAME_DIGITAL, INPUT);
    analogReadResolution(12);
    setLeds(false);
    
 

    const char* modelID = "XIAO-C6-StoveSmoke";
    const char* mfName  = "Seeed";

    // Przypisanie tożsamości
    zbFlameRaw.setManufacturerAndModel(mfName, modelID);
    zbSgpRaw.setManufacturerAndModel(mfName, modelID);
    zbFlameStat.setManufacturerAndModel(mfName, modelID);

    zbSgpStat.setManufacturerAndModel(mfName, modelID);
    zbSgpThreshold.setManufacturerAndModel(mfName, modelID);

    zbTempHum.setManufacturerAndModel(mfName, modelID); 

    // Konfiguracja Suwaka (EP5) - przed addEndpoint!
    zbSgpThreshold.addAnalogOutput(); 
        zbSgpThreshold.onAnalogOutputChange(myThresholdCallback);
        zbSgpThreshold.setAnalogOutputMinMax(0.0, 60000.0);
    // Konfiguracja Binary (EP3, EP4)
    zbFlameStat.addBinaryInput();
    zbFlameStat.setBinaryInputApplication(BINARY_INPUT_APPLICATION_TYPE_SECURITY_HEAT_DETECTION);   
    zbSgpStat.addBinaryInput();
    zbSgpStat.setBinaryInputApplication(BINARY_INPUT_APPLICATION_TYPE_SECURITY_SMOKE_DETECTION);
    // Konfiguracja EP6
    zbTempHum.addHumiditySensor();

    // Rejestracja Endpointów (Każdy TYLKO raz)
    Zigbee.addEndpoint(&zbFlameRaw);
    Zigbee.addEndpoint(&zbSgpRaw);
    Zigbee.addEndpoint(&zbFlameStat);
    Zigbee.addEndpoint(&zbSgpStat);
    Zigbee.addEndpoint(&zbSgpThreshold);
    Zigbee.addEndpoint(&zbTempHum);
   

    if (!Zigbee.begin(ZIGBEE_ROUTER)) {
        Serial.println("Zigbee Error! Restart...");
        delay(3000);
        ESP.restart();
    }
    // po inicjacji Zigbee ustalamy domyslna wartosc na suwaku
    zbSgpThreshold.setAnalogOutput(20000.0);

    if (!Zigbee.connected()) {
        isPairingMode = true;
    }
}

void loop() {
    // 1. Tryb parowania
    if (isPairingMode && !Zigbee.connected()) {
        if (millis() - lastBlinkTime > 500) {
            static bool bState = false;
            bState = !bState;
            setLeds(bState);
            lastBlinkTime = millis();
        }
    } else if (isPairingMode && Zigbee.connected()) {
        isPairingMode = false;
        setLeds(false);
        zigbeeRaportNow = true;
    }

    // 2. Logika pomiarowa
    if (millis() - lastMeasureTime > 1000) {
        int fAna = analogRead(PIN_FLAME_ANALOG);
        bool fDig = (digitalRead(PIN_FLAME_DIGITAL) == LOW);
        
        uint16_t srawVoc = 0;
        uint16_t err = sgp40.measureRawSignal(5000, 2500, srawVoc);

        float temperature = shtc3.getTemperature(PRECISION_HIGH_CLKSTRETCH_OFF);
        float humidity = shtc3.getHumidity(PRECISION_HIGH_CLKSTRETCH_OFF);

        if (Zigbee.connected()) {
            if ( zigbeeRaportNow || millis() > lastZigbeeRaport + 15*60*1000 || abs(fAna-last_fAna) > 100 || abs(srawVoc-last_srawVoc) > 100 || fDig != last_fDig || abs(temperature - last_temp) > 0.5 || abs(humidity - last_hum) > 5.0  ) 
            {
                zbFlameRaw.setIlluminance(fAna);
                if (err == 0) zbSgpRaw.setIlluminance(srawVoc);
                
                zbFlameStat.setBinaryInput(fDig);                 
                        
                bool sStat = (srawVoc < dynamicSgpThreshold && srawVoc > 0);
                zbSgpStat.setBinaryInput(sStat);

                zbTempHum.setTemperature(temperature);
                zbTempHum.setHumidity(humidity);



                zbFlameRaw.report();
                zbSgpRaw.report();
                zbFlameStat.reportBinaryInput();
                zbSgpStat.reportBinaryInput();
                zbTempHum.report();             
                
                lastZigbeeRaport = millis();                    
                last_fDig = fDig;
                last_fAna = fAna;
                last_srawVoc = srawVoc;
                last_temp = temperature;
                last_hum = humidity;
                zigbeeRaportNow = false;

                Serial.printf("*** Zigbee Send: Flame:%d SGP:%d Prog:%d Stat:%s | T: %.2f *C | H: %.2f %%\n", fAna, srawVoc, dynamicSgpThreshold, sStat ? "ALARM" : "OK", temperature, humidity);
                LED_Raport();
            } 
            else if (millis() > lastZigbeeAlive + 15000) {
                zbSgpStat.reportBinaryInput(); // Minimalny ruch w sieci
                lastZigbeeAlive = millis();
                LED_Raport(100);
            }
        }

        Serial.printf("Flame: %d | SGP: %d | Prog: %d | T: %.2f *C | H: %.2f %%\n", fAna, srawVoc, dynamicSgpThreshold, temperature, humidity);
        lastMeasureTime = millis();
    }

    // 3. Przycisk
    bool btn = (digitalRead(PIN_GROVE_BUTTON) == LOW);
    if (btn && !lastBtnState) {
        Serial.println("Klik!");
        LED_Raport(500);
        zigbeeRaportNow = true; 

        static uint32_t holdStart = 0;
        holdStart = millis();
        while(digitalRead(PIN_GROVE_BUTTON) == LOW) {
            if (millis() - holdStart > 10000) {
                Zigbee.factoryReset();
            }
        }
    }
    lastBtnState = btn;

    LED_Raport_Clear();
}