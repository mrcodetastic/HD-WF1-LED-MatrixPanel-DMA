// Pin Definitions for the Huidu HUB75 Series Control Card:  HD-WF1
// https://www.aliexpress.com/item/1005005038544582.html can be got for about $5 USD
#include "hd-wf1-esp32s2-config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <I2C_BM8563.h>   // https://github.com/tanakamasayuki/I2C_BM8563
#include <AutoConnect.h>  // https://github.com/Hieromon/AutoConnect
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ElegantOTA.h> // upload firmware by going to http://<ipaddress>/update
#include <esp_err.h>
#include <esp_log.h>
#include "debug.h"
#include "littlefs_core.h"
#include <ESP32Time.h>
#include <ctime>

#define fs LittleFS


/*----------------------------- RTC and NTP -------------------------------*/
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire1);
const char* ntpServer         = "time.cloudflare.com";
const char* ntpLastUpdate     = "/ntp_last_update.txt";


/*-------------------------- HUB75E DMA Setup -----------------------------*/
#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

HUB75_I2S_CFG::i2s_pins _pins_x1 = {WF1_R1_PIN, WF1_G1_PIN, WF1_B1_PIN, WF1_R2_PIN, WF1_G2_PIN, WF1_B2_PIN, WF1_A_PIN, WF1_B_PIN, WF1_C_PIN, WF1_D_PIN, WF1_E_PIN, WF1_LAT_PIN, WF1_OE_PIN, WF1_CLK_PIN};


/*-------------------------- Class Instances ------------------------------*/
// Routing in the root page and webcamview.html natively uses the request
// handlers of the ESP32 WebServer class, so it explicitly instantiates the
// ESP32 WebServer.
WebServer           webServer;
AutoConnect         Portal(webServer);
AutoConnectConfig   PortalConfig;
WiFiClient          client;
HTTPClient          http; 
ESP32Time           esp32rtc;  // offset in seconds GMT+1
MatrixPanel_I2S_DMA *dma_display = nullptr;

// ROS Task management
TaskHandle_t Task1;
TaskHandle_t Task2;

#include "graphics.h"
#include "timer_handler.h"
#include "led_pwm_handler.h"



// This gets called if captive portal is required.
bool atDetect(IPAddress& softapIP) 
{

  Sprintln(F(" * No WiFi configuration found. Starting Portal."));

  // AP will be availabe on your mobile phone, use the password 12345678 to connect
  Sprintln(F("Captive Portal started."));
  
  return true;
}

// Function that gets current epoch time
unsigned long getEpochTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void setup() {

  delay(2000);

  // Init Serial
  // if ARDUINO_USB_CDC_ON_BOOT is defined then the debug will go out via the serial port and not via ftdi.
  // ESP_LOG messages seem to go out via ftdi however (even if ARDUINO_USB_CDC_ON_BOOT is defined)
  Serial.begin(115200);

  delay(50);

  // Interrupt on toggle button press
  pinMode(PUSH_BUTTON_PIN, INPUT_PULLUP); // Set pin 11 as input with internal pull-up resistor
  attachInterrupt(digitalPinToInterrupt(PUSH_BUTTON_PIN), toggleButtonPressed, RISING); // Set up the interrupt on rising edge of pin 11

  // Setup LEDC PWM Controler
  ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
  ledcAttachPin(RUN_LED_PIN, LED_CHANNEL);  

  // Start fading that LED
  xTaskCreatePinnedToCore(
    ledFadeTask,            /* Task function. */
    "ledFadeTask",                 /* name of task. */
    1000,                    /* Stack size of task */
    NULL,                     /* parameter of the task */
    1,                        /* priority of the task */
    &Task1,                   /* Task handle to keep track of created task */
    0);                       /* Core */   
  

  /*-------------------- INIT LITTLE FS --------------------*/
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      Serial.println("LittleFS Mount Failed");
      return;
  }
  listDir(LittleFS, "/", 1);    
 
  /*-------------------- START THE NETWORKING --------------------*/
  Sprintln(F(" * Starting WiFi Connection Manager"));

  PortalConfig.apid = "MatixPanel";
  PortalConfig.title = "Configure WiFi";
  PortalConfig.menuItems = AC_MENUITEM_CONFIGNEW;

  Portal.config(PortalConfig);

  // Starts user web site included the AutoConnect portal.
  Portal.onDetect(atDetect);
  if (Portal.begin()) {
    //Serial.println("Started, IP:" + WiFi.localIP().toString());
  } else {
    Sprintln(F("Something went wrong?"));
    while (true) { 
      #if defined(ESP8266)          
              yield(); // keep the watchdog fed. Removed: NOT relevant to ESP32
      #endif         
      } // Wait.
  }
  // Connect to an access point
  // WiFi.begin();                 // Connect to the access point of the last connection
  // WiFi.begin(ssid, password);  // Or, Connect to the specified access point

  Serial.print("Connecting to Wi-Fi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  


  // Init I2C for RTC
  Wire1.begin(BM8563_I2C_SDA, BM8563_I2C_SCL);
  rtc.begin();

  // Get RTC date and time
  I2C_BM8563_DateTypeDef rtcDate;
  I2C_BM8563_TimeTypeDef rtcTime;
  rtc.getDate(&rtcDate);
  rtc.getTime(&rtcTime);
  
  time_t ntp_last_update_ts = 0;
  File file = fs.open(ntpLastUpdate, FILE_READ, true);
  if(!file) {
      Serial.println("failed to open file for reading");
  } else  {
      file.read( (uint8_t*) &ntp_last_update_ts, sizeof(ntp_last_update_ts));          
      Serial.print("Epoch read from file: ");
      Serial.println(ntp_last_update_ts);
      file.close();      
  }

  // Current RTC
  std::tm curr_rtc_tm = make_tm(rtcDate.year, rtcDate.month, rtcDate.date);    // April 2nd, 2012
  time_t  curr_rtc_ts = std::mktime(&curr_rtc_tm);

  if ( std::abs( (long int) (curr_rtc_ts - ntp_last_update_ts)) > (60*60*24*30) )
  {
      ESP_LOGI("need_ntp_update", "Longer than 30 days since last NTP update. Performing check.");    
  
      Serial.println("Updating RTC from Internet NTP.");  

      // Set ntp time to local
      configTime(CLOCK_GMT_OFFSET * 3600, 0, ntpServer);

      // Get local time
      struct tm timeInfo;
      if (getLocalTime(&timeInfo)) {
        // Set RTC time
        I2C_BM8563_TimeTypeDef timeStruct;
        timeStruct.hours   = timeInfo.tm_hour;
        timeStruct.minutes = timeInfo.tm_min;
        timeStruct.seconds = timeInfo.tm_sec;
        rtc.setTime(&timeStruct);

        // Set RTC Date
        I2C_BM8563_DateTypeDef dateStruct;
        dateStruct.weekDay = timeInfo.tm_wday;
        dateStruct.month   = timeInfo.tm_mon + 1;
        dateStruct.date    = timeInfo.tm_mday;
        dateStruct.year    = timeInfo.tm_year + 1900;
        rtc.setDate(&dateStruct);
    }

      ntp_last_update_ts = getEpochTime();
      File file = fs.open(ntpLastUpdate, FILE_WRITE);
      if(!file) {
          Serial.println("- failed to open file for writing");
      } else  {
          file.write( (uint8_t*) &ntp_last_update_ts, sizeof(ntp_last_update_ts));          
          file.close();      
          Serial.print("Wrote epoc time of: "); Serial.println(ntp_last_update_ts, DEC);            
      }

  } else {

    esp32rtc.setTime(rtcTime.seconds, rtcTime.minutes, rtcTime.hours, rtcDate.date, rtcDate.month, rtcDate.year);  // 17th Jan 2021 15:24:30
    Serial.println("Have a valid year on the external RTC. Updating ESP32 RTC to:");    
    Serial.println(esp32rtc.getTime("%A, %B %d %Y %H:%M:%S"));   // (String) returns time with specified format 
  }


	// Setup timer to call  checkToggleButtonTask every second
	if (ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS * 1000, timerCheckToggleButton))
	{
		Serial.print(F("Starting  timerCheckToggleButton OK, millis() = "));
		Serial.println(millis());
	}
	else
  {
  Serial.println(F("Can't set timerCheckToggleButton. Select another freq. or timer"));
  }


  /*-------------------- START THE HUB75E DISPLAY --------------------*/
  
  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN,   // Chain length
    _pins_x1       // pin mapping for port X1
  );
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;  
  mxconfig.latch_blanking = 3;
  //mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  //mxconfig.double_buff = false;  
  mxconfig.min_refresh_rate = 30;


  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128); //0-255
  dma_display->clearScreen();
  //dma_display->fillScreen(myWHITE);
  
  // fix the screen with green
  dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(0, 15, 0));
  delay(500);


  // fix the screen with green
  dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(0, 15, 0));
  delay(500);

  // draw a box in yellow
  dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(15, 15, 0));
  delay(500);

  // draw an 'X' in red
  dma_display->drawLine(0, 0, dma_display->width()-1, dma_display->height()-1, dma_display->color444(15, 0, 0));
  dma_display->drawLine(dma_display->width()-1, 0, 0, dma_display->height()-1, dma_display->color444(15, 0, 0));
  delay(500);

  // draw a blue circle
  dma_display->drawCircle(10, 10, 10, dma_display->color444(0, 0, 15));
  delay(500);

  // fill a violet circle
  dma_display->fillCircle(40, 21, 10, dma_display->color444(15, 0, 15));
  delay(500);  


    webServer.on("/", []() {
      webServer.send(200, "text/plain", "Hi! I am here.");
    });

    ElegantOTA.begin(&webServer);    // Start ElegantOTA
    webServer.begin();
    Serial.println("OTA HTTP server started");

    dma_display->print("test");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  

    delay(1000);
    
    dma_display->clearScreen();
    dma_display->setCursor(0,0);

    dma_display->print(WiFi.localIP());

    delay(3000);

}

unsigned long last_update = 0;
char buffer[64];
void loop() 
{
    webServer.handleClient();
    delay(1);

    if ( (millis() - last_update) > 1000 ) {

      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {

        //Serial.println("Failed to obtain time");
        dma_display->clearScreen();

        dma_display->setCursor(8,10);

        memset(buffer, 0, 64);
        snprintf(buffer, 64, "%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min, timeinfo.tm_sec);
        dma_display->print(buffer);

        Serial.println("Performing screen time update...");

      } else {
        Serial.println("Failed to get local time.");
      }

      last_update = millis();

    }


/*
  I2C_BM8563_DateTypeDef dateStruct;
  I2C_BM8563_TimeTypeDef timeStruct;

  // Get RTC
  rtc.getDate(&dateStruct);
  rtc.getTime(&timeStruct);

  // Print RTC
  Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n",
                dateStruct.year,
                dateStruct.month,
                dateStruct.date,
                timeStruct.hours,
                timeStruct.minutes,
                timeStruct.seconds
               );

  // Wait
  delay(1000);

  */

} // loop() 