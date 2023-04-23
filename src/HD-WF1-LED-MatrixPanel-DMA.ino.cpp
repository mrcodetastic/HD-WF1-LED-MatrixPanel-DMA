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


#define Sprintln(a) (Serial.println(a)) 
#define SprintlnDEC(a, x) (Serial.println(a, x))


/*-------------------------- LITTLEFS LIBRARIES ---------------------------*/
#include "LittleFSCore.h"


/*----------------------------- RTC and NTP -------------------------------*/
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire1);
const char* ntpServer = "time.cloudflare.com";


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
MatrixPanel_I2S_DMA *dma_display = nullptr;
volatile bool       buttonPressed = false;

TaskHandle_t LedTask1;

#include "graphics.h"
#include "timer_handler.h"
#include "led_pwm_handler.h"

void toggleButtonPressed() {
  // This function will be called when the interrupt occurs on pin 11
  buttonPressed = true;
  // Do something here
}


// This gets called if captive portal is required.
bool atDetect(IPAddress& softapIP) 
{

  Sprintln(F(" * No WiFi configuration found. Starting Portal."));

  // AP will be availabe on your mobile phone, use the password 12345678 to connect
  Sprintln(F("Captive Portal started."));
  
  return true;
}

void setup() {

  // Init Serial
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
  Serial.println( "SPIFFS-like write file to new path and delete it w/folders" );
  //writeFile2(LittleFS, "/new1/new2/new3/hello3.txt", "Hello3");
  //listDir(LittleFS, "/", 3);
  //deleteFile2(LittleFS, "/new1/new2/new3/hello3.txt");
  //createDir(LittleFS, "/mydir");
  writeFile(LittleFS, "/last_ntp_update.txt", "123456789");
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
  //WiFi.begin(ssid, password);  // Or, Connect to the specified access point

  Serial.print("Connecting to Wi-Fi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // Init I2C for RTC
  Wire1.begin(BM8563_I2C_SDA, BM8563_I2C_SCL);
  rtc.begin();

  // If we're 2023 or later then we can just use the RTC
  I2C_BM8563_DateTypeDef rtcDate;
  rtc.getDate(&rtcDate);
  
  if (rtcDate.year >= 2023)
  {
    Serial.println("Have a valid year on the RTC, so not going to make internet call.");
  }
  else 
  {
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

}

void loop() 
{

  Serial.println("Loop..");


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
}