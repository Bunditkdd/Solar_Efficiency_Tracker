# ☀️ Solar Efficiency & Pump Monitoring Data Logger
![Status](https://img.shields.io/badge/Status-Development-orange) ![Platform](https://img.shields.io/badge/Platform-Arduino%20/%20PlatformIO-blue) ![Hardware](https://img.shields.io/badge/Hardware-ESP32%20/%20Atmega-green)

ระบบบันทึกข้อมูลประสิทธิภาพพลังงานแสงอาทิตย์และสถานะปั๊มน้ำอัตโนมัติ บันทึกข้อมูลลง SD Card ในรูปแบบ CSV ทุก 15 นาที โดยจำกัดช่วงเวลาทำงานเฉพาะช่วงกลางวัน (06:00 - 18:00)

## 🛠 เซนเซอร์และอุปกรณ์ที่ใช้ (Hardware Architecture)
ระบบนี้ประกอบด้วยการวัดค่าจากเซนเซอร์หลายมิติเพื่อวิเคราะห์ประสิทธิภาพพลังงาน:

| ระบบ | อุปกรณ์ | พารามิเตอร์ที่วัด | การเชื่อมต่อ |
| :--- | :--- | :--- | :--- |
| **Solar Power** | **PZEM-017** | Voltage, Current, Power, Energy (DC) | Serial2 (Modbus RTU) |
| **Pump Power** | **INA219** | Voltage, Current, Power (Bus side) | I2C (Address 0x40) |
| **Environment** | **SHT31** | Temperature (°C), Humidity (%) | I2C (Address 0x44) |
| **Air Quality** | **PMS5003** | PM2.5, PM10 | Serial1 |
| **Light Level** | **BH1750** | Light Intensity (Lux) | I2C |
| **Water Flow** | **FS300A** | Flow Rate (L/min), Total Liters | Digital Pin 2 (Interrupt) |
| **Pressure** | **Analog Sensor**| Water Pressure (Bar, PSI) | Analog Pin A0 |
| **Time/Storage**| **DS1302 & SD** | RTC Time & Data Logging | SPI & ThreeWire |

## 🚀 ฟีเจอร์เด่น (Key Features)
* **High-Frequency Flow Measurement:** คำนวณอัตราการไหลของน้ำทุกๆ **1 วินาที** โดยใช้ระบบ Interrupt และ Delta Time เพื่อความแม่นยำสูงสุด
* **Smart Energy Logging:** บันทึกข้อมูลรวมลง SD Card ทุกๆ **15 นาที** โดยระบบจะตรวจสอบเวลาจาก RTC เพื่อทำงานเฉพาะช่วง 06:00 - 18:00 น. เท่านั้น
* **Daily CSV Logging:** สร้างไฟล์ข้อมูลแยกรายวันโดยอัตโนมัติ (เช่น `260309.csv`) พร้อมระบบตรวจสอบความพร้อมของ SD Card และ RTC ก่อนเขียนไฟล์
* **Standardized Timestamp:** ใช้รูปแบบเวลา `YYYY-MM-DDTHH:MM:SS` (ISO Format) เพื่อความสะดวกในการนำข้อมูลไปวิเคราะห์ต่อในโปรแกรม Excel หรือ MATLAB



## 🔌 การต่อสายใช้งาน (Pin Map)
* **RTC DS1302:** CLK -> 7, DAT -> 5, RST -> 6
* **SD Card Module:** CS -> 4
* **Flow Sensor:** Data -> 2
* **Pressure Sensor:** Out -> A0
* **PZEM-017:** TX/RX -> Serial2
* **PMS5003:** TX/RX -> Serial1

## 📝 วิธีใช้งาน (Usage)
1. ติดตั้ง Library ที่จำเป็น: `PMS`, `BH1750`, `Adafruit_SHT31`, `INA219_WE`, `RTClib`, `RtcDS1302`
2. ตรวจสอบการเชื่อมต่อเซนเซอร์ I2C และ Serial ให้ถูกต้อง
3. อัปโหลดโค้ดผ่าน Arduino IDE หรือ PlatformIO
4. ข้อมูลจะเริ่มบันทึกลงใน SD Card ตามช่วงเวลาที่กำหนด

---
**พัฒนาโดย:** 
*Engineering Student | Specialized in Electrical Design & Power Systems*
