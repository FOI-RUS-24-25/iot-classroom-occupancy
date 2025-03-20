# 📡 Smart Classroom Occupancy Monitoring System

This project implements a **real-time classroom occupancy monitoring system** using an **ESP32 microcontroller**, **PIR motion sensor**, and **Azure cloud services**. It collects data, processes it through **Azure Functions**, and stores it in **Azure SQL Database**, making the data accessible via a **REST API**.

## 🎯 Features

✅ **Real-time presence detection** using a PIR motion sensor  
✅ **Visual status indication** via an RGB LED (Green = Available, Red = Occupied)  
✅ **MQTT-based data transmission** to **Azure IoT Hub**  
✅ **Azure Functions** for processing and storing telemetry data  
✅ **Azure SQL Database** for tracking historical usage  
✅ **REST API** to retrieve real-time and historical occupancy data  
✅ **Web dashboard** for occupancy analytics  

## 🛠 Hardware Setup

The system is built using an **ESP32 microcontroller**, a **PIR motion sensor**, and an **RGB LED** for visual indication.

### 🔹 Components:
- **ESP32 DevKit**
- **PIR Motion Sensor**
- **Common Anode RGB LED**
- **330Ω Resistor (for LED)**
- **Breadboard & Jumper Wires**

### 🔹 Circuit Diagram:
![ESP32 Prototype](https://github.com/user-attachments/assets/eb33962e-09be-4f08-80eb-be9e9d691615)

> The ESP32 is connected to a **PIR motion sensor** to detect movement and an **RGB LED** to visually indicate room occupancy.

## 🛠 System Components

- **ESP32** microcontroller
- **PIR sensor** for motion detection
- **RGB LED** for visual indication
- **MQTT communication** with **Azure IoT Hub**
- **Azure Event Hub** for data processing
- **Azure Functions** to process incoming messages
- **Azure SQL Database** to store occupancy data
- **Node.js REST API** for data access
- **JavaScript web dashboard** for visualization  

## 📡 System Architecture

### 🔹 IoT Device (ESP32)
- **Reads sensor data (motion detection)**
- **Controls RGB LED** (green for available, red for occupied)
- **Establishes a secure MQTT connection with Azure IoT Hub**
- **Sends occupancy status as JSON telemetry**
- **Handles MQTT callbacks for cloud-to-device messages**
- **Retries MQTT connection upon failure**

### 🔹 Cloud Processing (Azure)
- **IoT Hub receives telemetry**
- **Event Hub forwards messages to Azure Functions**
- **Azure Functions parse and store data in SQL**
- **SQL Database logs timestamps, statuses, and trends**
- **REST API allows clients to retrieve data**

### 🔹 Web Dashboard
- **Fetches room occupancy status**
- **Displays historical usage trends**
- **Calculates average occupancy duration**
- **Visualizes peak hours with charts**
- **Alerts if a room is occupied for too long**

## 🔌 Installation & Setup

### 1️⃣ IoT Device (ESP32)
#### 📦 Dependencies
Make sure you have the required libraries installed in **PlatformIO** or **Arduino IDE**:
```cpp
#include <Arduino.h>
#include <AzIoTSasToken.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "secrets.h"
```
#### ⚙️ Configuration
Edit `secrets.h` with your credentials:
```cpp
#define SSID "YourWiFiSSID"
#define PASSWORD "YourWiFiPassword"
#define IOT_HUB_HOST "your-iot-hub.azure-devices.net"
#define DEVICE_ID "your-device-id"
#define DEVICE_KEY "your-device-key"
#define FUNCTION_URL "https://your-function-url.azurewebsites.net"
```
#### 🚀 Flash & Run
1. Connect the ESP32 and upload the firmware
2. The device will:
   - Connect to WiFi
   - Sync time using **NTP**
   - Authenticate with **Azure IoT Hub**
   - Send occupancy data via **MQTT**
   - Publish **JSON telemetry** to Azure

---

### 2️⃣ Cloud Setup (Azure)
#### ☁️ **Azure IoT Hub**
1. Create an **IoT Hub** in **Azure Portal**
2. Register an IoT device and retrieve the **Connection String**
3. Enable **Event Hub-compatible endpoint** for telemetry forwarding

#### 🏢 **Azure SQL Database**
1. Deploy the SQL database using the schema:
```sql
CREATE TABLE Telemetry (
    ID INT IDENTITY(1,1),
    Status BIT,
    Timestamp DATETIME,
    PRIMARY KEY (ID)
);
```
2. Ensure the **Azure SQL Firewall** allows connections

#### ⚙️ **Azure Function**
1. Deploy **Azure Function App** using **C#/.NET**
2. Function extracts IoT data and writes it to SQL:
```csharp
public static void Run([IoTHubTrigger("messages/events", Connection = "AzureIoTHubConnectionString")] string message, ILogger log)
{
    var data = JsonConvert.DeserializeObject<Telemetry>(message);
    SaveToDatabase(data);
}
```

---

### 3️⃣ Web API & Dashboard
#### 🚀 Running the Node.js API
1. Install dependencies:
```sh
npm install
```
2. Configure `.env`:
```sh
DB_USER=your_db_user
DB_PASSWORD=your_db_password
DB_SERVER=your_db_server.database.windows.net
DB_NAME=your_db_name
```
3. Start the API:
```sh
npm start
```
4. Access API at `http://localhost:4000/api`

---

## 🔎 API Endpoints

| Method | Route                      | Description |
|--------|----------------------------|-------------|
| GET    | `/api/last-status`         | Get current room occupancy |
| GET    | `/api/last-occupied-time`  | Get last recorded occupancy time |
| GET    | `/api/peak-time`           | Retrieve most occupied hours |
| GET    | `/api/average-occupancy`   | Calculate average room occupancy time |
| GET    | `/api/daily-summary?date=YYYY-MM-DD` | Get usage summary for a specific day |

---

## 📊 Web Dashboard Features

- **Real-time occupancy display**
- **Historical data visualization**
- **Peak time analysis using charts**
- **Alerts for prolonged occupancy**
- **Daily summary reports**

#### 📸 Screenshots:

##### 🟢 Room Status - Available  
![Room Available](https://github.com/user-attachments/assets/afefc6b8-760c-4464-8389-7e9cf1169900)

##### 🔴 Room Status - Occupied  
![Room Occupied](https://github.com/user-attachments/assets/5af41f9d-3584-42c3-8b7a-266e207f5561)

##### ⚠️ Room Status - Occupied for Too Long (Alert)  
![Room Occupied Alert](https://github.com/user-attachments/assets/ef8db1f5-fb51-4dc6-9258-7e32436b7753)

##### 📅 Daily Usage Summaries  
![Daily Usage](https://github.com/user-attachments/assets/5ce472c9-6112-48a7-9b48-f6c9792ed45a)

##### 📊 Peak Time Report  
![Peak Time](https://github.com/user-attachments/assets/2b02b464-f91d-4c63-ba43-e07af9c54f20)

---

## 📌 Technologies Used

- **Embedded Development**: ESP32, PlatformIO, Arduino
- **Cloud Services**: Azure IoT Hub, Azure Functions, Azure SQL
- **Web Backend**: Node.js, Express.js, MSSQL
- **Frontend**: HTML, JavaScript, Chart.js, CSS

---

## 👨‍💻 Authors  

This project was developed as part of the **Networked Systems Development** course at the **Faculty of Organization and Informatics (FOI)** by:  

- **Martin Kelemen**  
- **Dora Kulaš**  
- **Larija Jukić**  
- **Lana Ljubičić**  
- **Vito Petrinjak**  
- **Luka Pošta**  

---
