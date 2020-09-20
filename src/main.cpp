/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date June 2018 - 2019
 * @brief HPMA115S0 sensor on ESP32 with bluetooth GATT notify server
 * @license GPL3
 */

#include <Arduino.h>
#include <ConfigApp.hpp>
#include <Sensors.hpp>
#include <GUIUtils.hpp>
#include <watchdog.hpp>
#include <bluetooth.hpp>
#include <wifi.hpp>
// #include <OTAHandler.h>
// #include <Wire.h>
// #include <numeric>
// #include "main.h"

// void showValues(int pm25, int pm10){
//   gui.displaySensorAverage(apm25); // it was calculated on bleLoop()
//   gui.displaySensorData(pm25, pm10, chargeLevel, humi, temp, rssi);
//   gui.displayLiveIcon();
//   saveDataForAverage(pm25, pm10);
//   WrongSerialData = false;
// }


/******************************************************************************
*  M A I N
******************************************************************************/

// void wrongDataState(){
//   setErrorCode(ecode_sensor_read_fail);
//   gui.displaySensorAverage(apm25);
//   gui.displaySensorData(0,0,chargeLevel,0.0,0.0,0);
// #ifdef HONEYWELL
//   Serial.print("-->[E][HPMA] !wrong data!");
//   #ifndef TTGO_TQ
//     hpmaSerial.end();
//   #endif
// #elif PANASONIC
//   Serial.print("-->[E][SNGC] !wrong data!");
//   #ifndef TTGO_TQ
//     hpmaSerial.end();
//   #endif
// #else
//   Serial.print("-->[E][SPS30] !wrong data!");
// #endif
//   statusOff(bit_sensor);
//   WrongSerialData = true;
//   sensorInit();
//   delay(500);
// }

// void statusLoop(){
//   if () {
//     Serial.print("-->[STATUS] ");
//     Serial.println(status.to_string().c_str());
//     updateStatusError();
//     wifiCheck();
//   }
//   gui.updateError(getErrorCode());
//   gui.displayStatus(wifiOn,true,deviceConnected,dataSendToggle);
//   if(triggerSaveIcon++<3) gui.displayPrefSaveIcon(true);
//   else gui.displayPrefSaveIcon(false);
//   if(dataSendToggle) dataSendToggle=false;
// }

void setup(){
#ifdef TTGO_TQ
  pinMode(IP5306_2, INPUT);
  pinMode(IP5306_3, INPUT);
#endif
  pinMode(21, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP);
  Serial.begin(115200);
  gui.displayInit();
  gui.showWelcome();
  cfg.init("canairio");
  Serial.println("\n== INIT SETUP ==\n");
  Serial.println("-->[INFO] ESP32MAC: "+String(cfg.deviceId));
  watchdogInit();  // enable timer for reboot in any loop blocker
  gui.welcomeAddMessage("Sensors test..");
  sensors.init();
  bleServerInit();
  gui.welcomeAddMessage("GATT server..");
  if(cfg.ssid.length()>0) gui.welcomeAddMessage("WiFi:"+cfg.ssid);
  else gui.welcomeAddMessage("WiFi radio test..");
  wifiInit();
  gui.welcomeAddMessage("CanAirIO API..");
  influxDbInit();
  apiInit();
  pinMode(LED,OUTPUT);
  gui.welcomeAddMessage("==SETUP READY==");
  delay(500);
}

void loop(){
  gui.pageStart();
  sensors.loop();    // read sensor data and showed it
  // averageLoop();   // calculated of sensor data average
  // humidityLoop();  // read AM2320
  // batteryloop();   // battery charge status
  bleLoop();       // notify data to connected devices
  wifiLoop();      // check wifi and reconnect it
  apiLoop();       // CanAir.io API publication
  influxDbLoop();  // influxDB publication
  // statusLoop();    // update sensor status GUI
  otaLoop();       // check for firmware updates
  gui.pageEnd();   // gui changes push
#ifdef HONEYWELL
  delay(500);
#elif PANASONIC
  delay(500);
#else
  delay(900);
#endif
  watchdogLoop();     // reset every 20 minutes with Wifion
}