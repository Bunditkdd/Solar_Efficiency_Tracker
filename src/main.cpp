// hello
// hello1
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <PMS.h>
#include <BH1750.h>
#include <Adafruit_SHT31.h>
#include <INA219_WE.h>
#include "RTClib.h"
#include <ThreeWire.h>  
#include <RtcDS1302.h>

// --- [1] Forward Declarations (ประกาศหัวฟังก์ชันเพื่อให้ C++ รู้จัก) ---
void ReadPMS5003();
void ReadBH1750();
void ReadPZEM017_Manual(); 
bool ReadSHT31();
bool ReadINA219();
void ReadFlowSensor();
void ReadPressureSensor();
void logDataToSD();
void updateGlobalData();
void pulseCounter();

// --- [2] Objects & Global Variables ---
PMS pms(Serial1);
PMS::DATA data;
BH1750 lightMeter;
Adafruit_SHT31 sht31 = Adafruit_SHT31();
INA219_WE ina219 = INA219_WE(0x40);
ThreeWire myWire(6, 5, 7); 
RtcDS1302<ThreeWire> Rtc(myWire);
const int chipSelect = 4;

// คำสั่ง Modbus RTU สำหรับ PZEM-017
byte pzemRequest[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x08, 0xF1, 0xCC};
byte pzemResponse[25];

#define DEBUG 1
#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

struct senserdata {
  uint16_t pm25, pm10;
  float lux, voltage_solar, current_solar, power_solar, energy_solar;
  float voltage_pump, current_pump, power_pump;
  float temp, hum, flowRate, totalLiters;
  float water_bar, water_psi;
};

// แยกตัวแปรเก็บค่าแต่ละเซนเซอร์
senserdata PMS5003_data, BH1750_data, PZEM017_data, SHT31_data, INA219_data, FS300A_data, Pressure_data;
// ตัวแปรกลางสำหรับรวมข้อมูล
senserdata mySensors;

volatile uint16_t pulseCount = 0;
float calibrationFactor = 5.5; 
static unsigned long lastTime = 0; // เก็บเวลาครั้งล่าสุดที่วัด

// --- [3] Interrupt Service Routine ---
void pulseCounter() {
    pulseCount++;
}

// --- [4] Setup ---
void setup() {
    Serial.begin(115200);
    Serial1.begin(9600); // PMS5003
    Serial2.begin(9600); // PZEM-017 (Direct Modbus)
    Wire.begin();
    lightMeter.begin();
    
    if (!sht31.begin(0x44)) DEBUG_PRINTLN("SHT31 Error!");
    if (!ina219.init()) DEBUG_PRINTLN("INA219 Error!");
    ina219.setShuntSizeInOhms(0.0010);

    Rtc.Begin();
    /*
    if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
    if (!Rtc.GetIsRunning()) {
        Rtc.SetIsRunning(true);
        Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    */
    if (!SD.begin(chipSelect)) {
        Serial.println("SD Card failed!");
    } else {
        Serial.println("System Ready.");
    }
    
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), pulseCounter, RISING);
}

// --- [5] Main Loop ---
unsigned long previousMillis = 0;
unsigned long previousMillis1 = 0;
const long interval = 900000; // 15 นาที (900,000 ms) - แก้จาก 1000 เพื่อความสมจริงในการใช้งาน
const long interval1 = 1000;
void loop() {
    unsigned long currentMillis = millis();
    
    // ตรวจสอบข้อมูล PMS ตลอดเวลา (Background read)
    if (pms.read(data)) {
        PMS5003_data.pm25 = data.PM_AE_UG_2_5;
        PMS5003_data.pm10 = data.PM_AE_UG_10_0;
    }

    if (currentMillis - previousMillis1 >= interval1) {
        previousMillis1 = currentMillis;
        ReadFlowSensor(); 
    }

    // ทำงานตามรอบเวลา 15 นาที
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        ReadPMS5003(); 
        ReadBH1750(); 
        ReadPZEM017_Manual(); 
        ReadSHT31();
        ReadINA219(); 
        //ReadFlowSensor(); 
        ReadPressureSensor();
        
        updateGlobalData();
        logDataToSD();
        DEBUG_PRINTLN("--- 15-Minute Logging Completed ---");
    }
}

// --- [6] Function Implementations ---

void ReadPMS5003() {
    // ใน loop มีการดึงค่าใส่ PMS5003_data ตลอดเวลาอยู่แล้ว 
    // ฟังก์ชันนี้จึงเน้นการ Debug ค่าล่าสุด
    DEBUG_PRINT("PMS -> PM2.5: "); DEBUG_PRINT(PMS5003_data.pm25);
    DEBUG_PRINT(" | PM10: "); DEBUG_PRINTLN(PMS5003_data.pm10);
}

void ReadBH1750() {
    BH1750_data.lux = lightMeter.readLightLevel();
    DEBUG_PRINT("BH1750 -> LUX: "); DEBUG_PRINTLN(BH1750_data.lux);
}

void ReadPZEM017_Manual() {
    while(Serial2.available()) Serial2.read(); 
    Serial2.write(pzemRequest, sizeof(pzemRequest));
    
    unsigned long startTime = millis();
    while (Serial2.available() < 21 && millis() - startTime < 1000) {
        delay(1);
    }

    if (Serial2.available() >= 21) {
        for (int i = 0; i < 21; i++) {
            pzemResponse[i] = Serial2.read();
        }
        PZEM017_data.voltage_solar = ((pzemResponse[3] << 8) | pzemResponse[4]) / 100.0;
        PZEM017_data.current_solar = ((pzemResponse[5] << 8) | pzemResponse[6]) / 100.0*0.45;
        
        uint32_t power_raw = ((uint32_t)pzemResponse[9] << 24) | ((uint32_t)pzemResponse[10] << 16) | 
                             ((uint32_t)pzemResponse[7] << 8) | pzemResponse[8];
        PZEM017_data.power_solar = power_raw / 10.0;

        uint32_t energy_raw = ((uint32_t)pzemResponse[13] << 24) | ((uint32_t)pzemResponse[14] << 16) | 
                              ((uint32_t)pzemResponse[11] << 8) | pzemResponse[12];
        PZEM017_data.energy_solar = energy_raw / 1000.0;
        
        DEBUG_PRINT("PZEM -> V: "); DEBUG_PRINTLN(PZEM017_data.voltage_solar);
        DEBUG_PRINT("PZEM -> I: "); DEBUG_PRINTLN(PZEM017_data.current_solar);
    } else {
        DEBUG_PRINTLN("PZEM Error: Timeout");
    }
}

bool ReadSHT31() {
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        SHT31_data.temp = t;
        SHT31_data.hum = h;
        DEBUG_PRINT("SHT31 -> Temp: "); DEBUG_PRINT(t); DEBUG_PRINTLN(" C");
        return true;
    }
    return false;
}

bool ReadINA219() {
    float v = ina219.getBusVoltage_V();
    float a = ina219.getCurrent_mA()/1000/1.5;
    if (!isnan(v)) {
        INA219_data.voltage_pump = v;
        INA219_data.current_pump = a;
        INA219_data.power_pump = v * a;
        DEBUG_PRINT("INA219 -> V_Pump: "); DEBUG_PRINTLN(v);
        DEBUG_PRINT("INA219 -> A_Pump: "); DEBUG_PRINTLN(a);
        DEBUG_PRINT("INA219 -> P_Pump: "); DEBUG_PRINTLN(INA219_data.power_pump);
        return true;
    }
    return false;
}

void ReadFlowSensor() {
    // 1. ดึงเวลาปัจจุบันมาคำนวณส่วนต่าง (Delta Time)
    unsigned long now = millis();
    float deltaTime = (now - lastTime) / 1000.0; // แปลงเป็นวินาที (เช่น 1.002s)
    
    // ป้องกันการหารด้วยศูนย์ กรณีฟังก์ชันถูกเรียกถี่เกินไป
    if (deltaTime <= 0) return; 

    // 2. หยุด Interrupt ชั่วคราวเพื่อคัดลอกค่า (Atomic Operation)
    // วิธีนี้แม่นยำกว่า detachInterrupt เพราะหยุดระบบแค่เสี้ยววินาที
    noInterrupts();
    long currentPulses = pulseCount;
    pulseCount = 0; // รีเซ็ตตัวนับทันที
    interrupts();

    // 3. คำนวณอัตราการไหล (L/min) 
    // สูตร: (จำนวน Pulse / ปัจจัยการปรับแก้) / เวลาที่ผ่านไปจริง
    FS300A_data.flowRate = (currentPulses / calibrationFactor) / deltaTime;

    // 4. คำนวณปริมาณน้ำสะสม (Total Liters)
    // สูตร: flowRate (L/min) คูณกับ เวลาที่ผ่านไป (นาที)
    FS300A_data.totalLiters += (FS300A_data.flowRate * (deltaTime / 60.0));

    // 5. อัปเดตเวลาล่าสุด
    lastTime = now;

    // --- ส่วนแสดงผล ---
    DEBUG_PRINT("Flow -> Rate: "); 
    DEBUG_PRINT(FS300A_data.flowRate);
    DEBUG_PRINT(" L/min | Total: ");
    DEBUG_PRINTLN(FS300A_data.totalLiters);
}

void ReadPressureSGensor() {
    int rawValue = analogRead(A0);
    float voltage = (rawValue * 5.0) / 1023.0;
    float bar = (voltage - 0.5) * 12.0 / (4.5 - 0.5);
    if (bar < 0) bar = 0;
    Pressure_data.water_bar = bar;
    Pressure_data.water_psi = bar * 14.5038;
    DEBUG_PRINT("Pressure -> Bar: "); DEBUG_PRINTLN(bar);
}

void updateGlobalData() {
    mySensors.pm25 = PMS5003_data.pm25;
    mySensors.pm10 = PMS5003_data.pm10;
    mySensors.lux = BH1750_data.lux;
    mySensors.voltage_solar = PZEM017_data.voltage_solar;
    mySensors.current_solar = PZEM017_data.current_solar;
    mySensors.power_solar = PZEM017_data.power_solar;
    mySensors.energy_solar = PZEM017_data.energy_solar;
    mySensors.voltage_pump = INA219_data.voltage_pump;
    mySensors.current_pump = INA219_data.current_pump;
    mySensors.power_pump = INA219_data.power_pump;
    mySensors.temp = SHT31_data.temp;
    mySensors.hum = SHT31_data.hum;
    mySensors.flowRate = FS300A_data.flowRate;
    mySensors.totalLiters = FS300A_data.totalLiters;
    mySensors.water_bar = Pressure_data.water_bar;
    mySensors.water_psi = Pressure_data.water_psi;
}

void logDataToSD() {
    if (!SD.exists("/")) { // ลองตรวจสอบ Root Directory
        Serial.println("SD Card lost! Attempting to reconnect...");
        if (!SD.begin(4)) { // ใส่ขา CS ที่คุณใช้ เช่น 4, 5, 10, 53
            Serial.println("Reconnection failed.");
            return; 
        }
        Serial.println("Reconnection successful!");
    }

    RtcDateTime now = Rtc.GetDateTime();

    // 2. ตรวจสอบความถูกต้องของปี (Validation)
    // หากปีต่ำกว่า 2025 แสดงว่านาฬิกายังไม่พร้อม หรือแบตเตอรี่ RTC มีปัญหา
    if (now.Year() < 2025) {
        Serial.println("Error: Invalid RTC Year! Skipping SD Log.");
        return; 
    }

    // 3. เช็คช่วงเวลาทำงาน (06:00 - 20:00)
    if (now.Hour() < 6 || now.Hour() >= 18) {
        Serial.println("Outside logging hours (06:00-20:00).");
        return; 
    }

    // 4. เตรียมชื่อไฟล์ (เช็คให้ชัวร์ว่าตัวแปร fileName สะอาด)
    char fileName[13];
    memset(fileName, 0, sizeof(fileName)); 
    sprintf(fileName, "%02d%02d%02d.csv", now.Year() % 100, now.Month(), now.Day());

    // 5. สร้าง Header ถ้าเป็นไฟล์ใหม่
    if (!SD.exists(fileName)) {
        File dataFile = SD.open(fileName, FILE_WRITE);
        if (dataFile) {
            dataFile.println("Timestamp,PM2.5,PM10,Lux,V_Solar,I_Solar,P_Solar,E_Solar,V_Pump,I_Pump,P_Pump,Temp,Hum,FlowRate,TotalLiters,Water_Bar,Water_Psi");
            dataFile.close();
            Serial.print("Created new file: "); Serial.println(fileName);
        }
    }

    // 6. เปิดไฟล์เพื่อเขียนข้อมูล
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
        char timestamp[35];
        sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d.000+07:00", 
                now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second());
        
        dataFile.print(timestamp); dataFile.print(",");
        
        // ข้อมูลเซนเซอร์ (เรียกใช้จากโครงสร้าง mySensors ที่ update ล่าสุดแล้ว)
        dataFile.print(mySensors.pm25); dataFile.print(",");
        dataFile.print(mySensors.pm10); dataFile.print(",");
        dataFile.print(mySensors.lux, 2); dataFile.print(",");
        dataFile.print(mySensors.voltage_solar, 2); dataFile.print(",");
        dataFile.print(mySensors.current_solar, 3); dataFile.print(",");
        dataFile.print(mySensors.power_solar, 2); dataFile.print(",");
        dataFile.print(mySensors.energy_solar, 2); dataFile.print(",");
        dataFile.print(mySensors.voltage_pump, 2); dataFile.print(",");
        dataFile.print(mySensors.current_pump, 3); dataFile.print(",");
        dataFile.print(mySensors.power_pump, 2); dataFile.print(",");
        dataFile.print(mySensors.temp, 2); dataFile.print(",");
        dataFile.print(mySensors.hum, 2); dataFile.print(",");
        dataFile.print(mySensors.flowRate, 2); dataFile.print(",");
        dataFile.print(mySensors.totalLiters, 2); dataFile.print(",");
        dataFile.print(mySensors.water_bar, 2); dataFile.print(",");
        dataFile.println(mySensors.water_psi, 2); // ใช้ println เพื่อจบแถว

        dataFile.close();
        Serial.print("Data logged to: "); Serial.println(fileName);
        Serial.print("miuint : "); Serial.print(now.Minute());
        Serial.print("secode : "); Serial.print(now.Second());
    } else {
        Serial.print("Error opening: "); Serial.println(fileName);
    }
}