/***************************************************************************************************************************************
                                                                                                                                                        
    Project:         Webspector - WebServer based Spectrum Analyzer
    Target Platform: ESP32                                                                                                                                                                                                                                                                                       *
    Version: 1.1
    Hardware setup: See github
                                                                                                                                                                                                                                                                                                            
    Mark Donners
    The Electronic Engineer
    Website:   www.theelectronicengineer.nl
    facebook:  https://www.facebook.com/TheelectronicEngineer
    youtube:   https://www.youtube.com/channel/UCm5wy-2RoXGjG2F9wpDFF3w
    github:    https://github.com/donnersm
                                                                                                                                                      
 *   This sketch consists of 2 files; this one and the Settings.h file. Make sure both are in the same directory!                                                                                                                                                  
 *   
 *   All settings that you can change are in the Settings.h file
 *   Do not change settings below
 ****************************************************************************************************************************************/

#define VERSION     "V1.1"

//included files
#include "Settings.h"

//general libaries
#include <arduinoFFT.h>                                 //libary for FFT analysis
#include <EasyButton.h>                                 //libary for handling buttons

//libaries for webinterface
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Ticker.h>
#include <WiFiManager.h>                                //The magic setup for wifi! If you need to setup your WIFI, hold the mode button during boot up.

//Driver for I2S ADC Conversion
#include <driver/i2s.h>
#include <driver/adc.h>


/*********************************************************/
//Variables and stuff that don't need changing          //*
const i2s_port_t I2S_PORT = I2S_NUM_0;                  //*
#define ADC_INPUT ADC1_CHANNEL_0                        //*
#define ARRAYSIZE(a)    (sizeof(a)/sizeof(a[0]))        //*
uint16_t offset = (int)ADC_INPUT * 0x1000 + 0xFFF;      //*
double vReal[SAMPLEBLOCK];                              //*
double vImag[SAMPLEBLOCK];                              //*
int16_t samples[SAMPLEBLOCK];                           //*
byte peak[65];                                          //*
int oldBarHeights[65];                                  //*
volatile float FreqBins[65];                            //*
float FreqBinsOld[65];                                  //*
float FreqBinsNew[65];                                  //*
/********************************************************/


//****************************************************************************************
// below is is function that is needed when changing the number of bands during runtime, do not change
void SetNumberofBands(int bandnumber) {
  switch (bandnumber)
  {
    case 8:
      for (int x = 0; x < bandnumber; x++)
      {
        BandCutoffTable[x] = BandCutoffTable8[x];
        labels[x] = labels8[x];
      }
      break;

    case 16:
      for (int x = 0; x < bandnumber; x++)
      {
        BandCutoffTable[x] = BandCutoffTable16[x];
        labels[x] = labels16[x];
      }
      break;

    case 24:
      for (int x = 0; x < bandnumber; x++)
      {
        BandCutoffTable[x] = BandCutoffTable24[x];
        labels[x] = labels24[x];
      }

      break;

    case 32:
      for (int x = 0; x < bandnumber; x++)
      {
        BandCutoffTable[x] = BandCutoffTable32[x];
        labels[x] = labels32[x];
      }

      break;
    case 64:
      for (int x = 0; x < bandnumber; x++)
      {
        BandCutoffTable[x] = BandCutoffTable64[x];
        labels[x] = labels64[x];
      }

      break;
  }

}



arduinoFFT FFT = arduinoFFT(); /* Create FFT object */


//****************************************************************************************
// below is the function to initialize the I2S functionality, do not change
void setupI2S() {
  Serial.println("Configuring I2S...");
  esp_err_t err;

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = samplingFrequency,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // could only get it to work with 32bits
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // although the SEL config should be left, it seems to transmit on right
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
    .dma_buf_count = 2,                           // number of buffers
    .dma_buf_len = SAMPLEBLOCK,                     // samples per buffer
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.

  err = adc_gpio_init(ADC_UNIT_1, ADC_CHANNEL_0); //step 1
  if (err != ESP_OK) {
    Serial.printf("Failed setting up adc channel: %d\n", err);
    while (true);
  }

  err = i2s_driver_install(I2S_NUM_0, &i2s_config,  0, NULL);  //step 2
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }

  err = i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0);
  if (err != ESP_OK) {
    Serial.printf("Failed setting up adc mode: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver installed.");
}
//****************************************************************************************

//************* web server setup *************************************************************************************************************************
TaskHandle_t WebserverTask;                             // setting up the task handler for webserver                                                  //**
bool      webtoken =          false;                    // this is a flag so that the webserver noise when the other core has new data                //**
WebServer server(80);                                   // more webserver stuff                                                                       //**
WiFiManager wm;                                         // Wifi Manager init                                                                          //**
WebSocketsServer webSocket = WebSocketsServer(81);      // Adding a websocket to the server                                                           //**
//************* web server setup end**********************************************************************************************************************

//*************Button setup ******************************************************************************************************************************
EasyButton ModeBut(MODE_BUTTON_PIN);                    //defining the button                                                                         //**
// Mode button 1 short press                                                                                                                          //**
// will result in changing the number of bands                                                                                                        //**
void onPressed() {                                                                                                                                    //**
  Serial.println("Mode Button has been pressed!");                                                                                                    //**
  if (numBands == 8)numBands = 16;                                                                                                                    //**
  else if (numBands == 16)numBands = 24;                                                                                                              //**
  else if (numBands == 24)numBands = 32;                                                                                                              //**
  else if (numBands == 32)numBands = 64;                                                                                                              //**
  else if (numBands == 64)numBands = 8;                                                                                                               //**
  SetNumberofBands(numBands);                                                                                                                         //**
  Serial.printf("New number of bands=%d\n", numBands);

}                                                                                                                                                     //**
//*************Button setup end***************************************************************************************************************************


//****************************************************************************************
// Below is the Setup function that is run once on startup 

void setup() {
  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  // this will run the webinterface datatransfer.
  xTaskCreatePinnedToCore(
    Task1code,                                          /* Task function. */
    "WebserverTask",                                    /* name of task. */
    10000,                                              /* Stack size of task */
    NULL,                                               /* parameter of the task */
    4,                                                  /* priority of the task */
    &WebserverTask,                                     /* Task handle to keep track of created task */
    0);                                                 /* pin task to core 0 */

  delay(500);
  Serial.begin(115200);
  Serial.println("Setting up Audio Input I2S");
  setupI2S();
  delay(100);
  i2s_adc_enable(I2S_NUM_0);
  Serial.println("Audio input setup completed");
  ModeBut.onPressed(onPressed);


  if (digitalRead(MODE_BUTTON_PIN) == 0) {              //reset saved settings is mode button is pressed and hold during startup
    Serial.println("button pressed on startup, WIFI settings will be reset");
    wm.resetSettings();
  }

  wm.setConfigPortalBlocking(false);                    //Try to connect WiFi, then create AP but if no success then don't block the program
  // If needed, it will be handled in core 0 later
  wm.autoConnect("ESP32_AP", "");

  Serial.println(Projectinfo);                          // print some info about the project
  server.on("/", []() {                                 // this will load the actual html webpage to be displayed
    server.send_P(200, "text/html", webpage);
  });

  server.begin();                                       // now start the server
  Serial.println("HTTP server started");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  SetNumberofBands(numBands);
}
//****************************************************************************************


//****************************************************************************************
// Main loop running on 1 core while the web update is running on another core
void loop() {
  size_t bytesRead = 0;
  int TempADC = 0;
  ModeBut.read();

  //############ Step 1: read samples from the I2S Buffer ##################
  i2s_read(I2S_PORT,
           (void*)samples,
           sizeof(samples),
           &bytesRead,   // workaround This is the actual buffer size last half will be empty but why?
           portMAX_DELAY); // no timeout

  if (bytesRead != sizeof(samples)) {
    Serial.printf("Could only read %u bytes of %u in FillBufferI2S()\n", bytesRead, sizeof(samples));
  }

  //############ Step 2: compensate for Channel number and offset, safe all to vReal Array   ############
  for (uint16_t i = 0; i < ARRAYSIZE(samples); i++) {
    vReal[i] = offset - samples[i];
    vImag[i] = 0.0; //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
  }

  //############ Step 3: Do FFT on the VReal array  ############
  // compute FFT
  FFT.DCRemoval();
  FFT.Windowing(vReal, SAMPLEBLOCK, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLEBLOCK, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLEBLOCK);
  FFT.MajorPeak(vReal, SAMPLEBLOCK, samplingFrequency);
  for (int i = 0; i < numBands; i++) {
    FreqBins[i] = 0;
  }
  
  //############ Step 4: Fill the frequency bins with the FFT Samples ############
  for (int i = 2; i < SAMPLEBLOCK / 2; i++) {
    if (vReal[i] > NoiseTresshold) {
      int freq = BucketFrequency(i);
      int iBand = 0;
      while (iBand < numBands) {
        if (freq < BandCutoffTable[iBand])break;
        iBand++;
      }
      if (iBand > numBands)iBand = numBands;
      FreqBins[iBand] += vReal[i];
    }
  }


  //############ Step 5: Averaging and making it all fit on screen
  static float lastAllBandsPeak = 0.0f;
  float allBandsPeak = 0;
  for (int i = 0; i < numBands; i++) {
    if (FreqBins[i] > allBandsPeak) {
      allBandsPeak = FreqBins[i];
    }
  }
  if (allBandsPeak < 1)allBandsPeak = 1;
  allBandsPeak = max(allBandsPeak, ((lastAllBandsPeak * (GAIN_DAMPEN - 1)) + allBandsPeak) / GAIN_DAMPEN); // Dampen rate of change a little bit on way down
  lastAllBandsPeak = allBandsPeak;
  if (allBandsPeak < 80000)allBandsPeak = 80000;
  for (int i = 0; i < numBands; i++)FreqBins[i] /= (allBandsPeak * 1.0f);
  webtoken = true;                  // set marker so that other core can process data
} // loop end
//****************************************************************************************


//****************************************************************************************
// Return the frequency corresponding to the Nth sample bucket.  Skips the first two
// buckets which are overall amplitude and something else.
int BucketFrequency(int iBucket) {
  if (iBucket <= 1)return 0;
  int iOffset = iBucket - 2;
  return iOffset * (samplingFrequency / 2) / (SAMPLEBLOCK / 2);
}

//****************************************************************************************
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Do something with the data from the client
  if (type == WStype_TEXT) {
    Serial.println("websocket event Triggered");
  }
}
//****************************************************************************************

//****************************************************************************************
void SendData() {
  String json = "[";
  for (int i = 0; i < numBands; i++) {
    if (i > 0) {
      json += ", ";
    }
    json += "{\"bin\":";
    json += "\"" + labels[i] + "\"";
    json += ", \"value\":";
    json += String(FreqBins[i]);
    json += "}";
  }
  json += "]";
  webSocket.broadcastTXT(json.c_str(), json.length());
}
//****************************************************************************************


//****************************************************************************************
//Task1code: webserver runs on separate core so that WIFI low signal doesn't freeze up program on other core
void Task1code( void * pvParameters ) {
  delay(3000);
  Serial.print("Webserver task is  running on core ");
  Serial.println(xPortGetCoreID());
  int gHue = 0;
  for (;;) {
    wm.process();
    webSocket.loop();
    server.handleClient();
    if (webtoken == true) {
      // lets smooth out the data we send
      for (int cnt = 0; cnt < numBands; cnt++) {
        FreqBinsNew[cnt] = FreqBins[cnt];
        if (FreqBinsNew[cnt] < FreqBinsOld[cnt]) {
          FreqBins[cnt] = max(FreqBinsOld[cnt] - Speedfilter, FreqBinsNew[cnt]);
          if (FreqBins[cnt] > 1.0)FreqBins[cnt] = 1; //to prevent glitch when changing number of channels during runtime
        }
        else if (FreqBinsNew[cnt] > FreqBinsOld[cnt]) {
          FreqBins[cnt] = FreqBinsNew[cnt];
        }
        FreqBinsOld[cnt] = FreqBins[cnt];
      }
      // done smoothing now send the data
      SendData(); // webbrowser
      webtoken = false;
    }
  }
}
//****************************************************************************************
