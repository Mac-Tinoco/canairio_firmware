/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date June 2018 - 2020
 * @brief Particle meter sensor on ESP32 with bluetooth GATT notify server
 * @license GPL3
 */

#include <Arduino.h>

#include <Watchdog.hpp>
#include <ConfigApp.hpp>
#include <GUILib.hpp>
#include <Sensors.hpp>
#include <battery.hpp>
#include <bluetooth.hpp>
#include <wifi.hpp>

void refreshGUIData() {
    gui.displaySensorLiveIcon();  // all sensors read are ok
    int deviceType = sensors.getPmDeviceTypeSelected();
    uint16_t mainValue = 0;
    if (deviceType <= 3) {
        mainValue = sensors.getPM25();
    } else {
        mainValue = sensors.getCO2();
    }

    float humi = sensors.getHumidity();
    if (humi == 0.0) humi = sensors.getCO2humi();

    float temp = sensors.getTemperature();
    if (temp == 0.0) temp = sensors.getCO2temp();

    gui.setSensorData(
        mainValue,
        getChargeLevel(),
        humi,
        temp,
        getWifiRSSI(),
        deviceType);
}

class MyGUIUserPreferencesCallbacks : public GUIUserPreferencesCallbacks {
    void onWifiMode(bool enable){
        Serial.println("-->[MAIN] onWifi changed: "+String(enable));
        cfg.wifiEnable(enable);
        cfg.reload();
        if (!enable) wifiStop();
    };
    void onBrightness(int value){
        Serial.println("-->[MAIN] onBrightness changed: "+String(value));
        cfg.saveBrightness(value);
    };
    void onColorsInverted(bool enable){
        Serial.println("-->[MAIN] onColorsInverted changed: "+String(enable));
        cfg.colorsInvertedEnable(enable);
    };
    void onSampleTime(int time){
        if(sensors.sample_time != time) {
            Serial.println("-->[MAIN] onSampleTime changed: "+String(time));
            cfg.saveSampleTime(time);
            cfg.reload();
            sensors.setSampleTime(cfg.stime);
        } 
    };
    void onCalibrationReady(){
        Serial.println("-->[MAIN] onCalibrationReady");
        sensors.setCO2RecalibrationFactor(400);
    };
};

/// sensors data callback
void onSensorDataOk() {
    log_i("[MAIN] onSensorDataOk");
    refreshGUIData();
}

/// sensors error callback
void onSensorDataError(const char * msg){
    log_w("[MAIN] onSensorDataError", msg);
}

void startingSensors() {
    Serial.println("-->[INFO] PM sensor configured: "+String(cfg.stype));
    gui.welcomeAddMessage("Detected sensor:");
    sensors.setOnDataCallBack(&onSensorDataOk);     // all data read callback
    sensors.setOnErrorCallBack(&onSensorDataError); // on data error callback
    sensors.setSampleTime(2);                       // config sensors sample time (first use)
    sensors.setDebugMode(false);                    // [optional] debug mode
    sensors.init(cfg.getSensorType());              // start all sensors and
                                                    // try to detect configured PM sensor.
                                                    // Sensors PM2.5 supported: Panasonic, Honeywell, Plantower and Sensirion
                                                    // Sensors CO2 supported: Sensirion, Winsen, Cubic
                                                    // The configured sensor is choosed on Android app.
                                                    // For more information about the supported sensors,
                                                    // please see the canairio_sensorlib documentation.

    if(sensors.isPmSensorConfigured()){
        Serial.print("-->[INFO] PM/CO2 sensor detected: ");
        Serial.println(sensors.getPmDeviceSelected());
        gui.welcomeAddMessage(sensors.getPmDeviceSelected());
    }
    else {
        Serial.println("-->[INFO] Detection sensors FAIL!");
        gui.welcomeAddMessage("Detection !FAILED!");
    }
}

/******************************************************************************
*  M A I N
******************************************************************************/

void guiTask(void* pvParameters) {
    Serial.println("-->[INFO] config GUI task loop");
    while (1) {

        gui.pageStart();
        gui.displayMainValues();
        gui.displayStatus(WiFi.isConnected(), true, bleIsConnected());
        gui.pageEnd();

        delay(80);
    }
}

void setupGUITask() {
    xTaskCreatePinnedToCore(
        guiTask,    /* Function to implement the task */
        "tempTask ", /* Name of the task */
        10000,        /* Stack size in words */
        NULL,        /* Task input parameter */
        5,           /* Priority of the task */
        NULL,        /* Task handle. */
        1);          /* Core where the task should run */
}

void setup() {
    Serial.begin(115200);
    delay(400);
    Serial.println("\n== CanAirIO Setup ==\n");

    // init app preferences and load settings
    cfg.setDebugMode(false);
    cfg.init("canairio");

    // init graphic user interface
    gui.setBrightness(cfg.getBrightness());
    gui.setWifiMode(cfg.isWifiEnable());
    gui.setSampleTime(cfg.stime);
    gui.displayInit();
    gui.setCallbacks(new MyGUIUserPreferencesCallbacks());
    gui.showWelcome();


    // device wifi mac addres and firmware version
    Serial.println("-->[INFO] ESP32MAC: " + cfg.deviceId);
    Serial.println("-->[INFO] Revision: " + gui.getFirmwareVersionCode());
    Serial.println("-->[INFO] Firmware: " + String(VERSION));
    Serial.println("-->[INFO] Flavor  : " + String(FLAVOR));
    Serial.println("-->[INFO] Target  : " + String(TARGET));

    // init all sensors
    Serial.println("-->[INFO] Detecting sensors..");
    pinMode(PMS_EN, OUTPUT);
    digitalWrite(PMS_EN, HIGH);
    startingSensors();

    // init battery (only for some boards)
    batteryInit();

    // init watchdog timer for reboot in any loop blocker
    wd.init();

    // WiFi and cloud communication
    wifiInit();

    // Bluetooth low energy init (GATT server for device config)
    bleServerInit();
    gui.welcomeAddMessage("Bluetooth ready.");

    Serial.println("-->[INFO] InfluxDb API:\t" + String(cfg.isIfxEnable()));
    Serial.println("-->[INFO] CanAirIO API:\t" + String(cfg.isApiEnable()));
    gui.welcomeAddMessage("CanAirIO API:"+String(cfg.isApiEnable()));
    gui.welcomeAddMessage("InfluxDb :"+String(cfg.isIfxEnable()));

    influxDbInit();     // Instance DB handler
    apiInit();          // DEPRECATED

    // wifi status 
    if (WiFi.isConnected())
        gui.welcomeAddMessage("WiFi:" + cfg.ssid);
    else
        gui.welcomeAddMessage("WiFi: disabled.");

    // sensor sample time and publish time (2x)
    gui.welcomeAddMessage("stime: "+String(cfg.stime)+ " sec.");
    gui.welcomeAddMessage(cfg.getDeviceId());   // mac address
    gui.welcomeAddMessage("Watchdog:"+String(WATCHDOG_TIME));
    gui.welcomeAddMessage("==SETUP READY==");
    delay(1000);
    gui.showMain();
    refreshGUIData();
    delay(1500);
    sensors.loop();
    sensors.setSampleTime(cfg.stime);        // config sensors sample time (first use)
    setupGUITask();
}

void loop() {

    sensors.loop();  // read sensor data and showed it
    batteryloop();   // battery charge status
    bleLoop();       // notify data to connected devices
    wifiLoop();      // check wifi and reconnect it
    apiLoop();       // CanAir.io API !! D E P R E C A T E D !!
    influxDbLoop();  // influxDB publication
    otaLoop();       // check for firmware updates
    wd.loop();       // watchdog for check loop blockers
    
    delay(500);
}
