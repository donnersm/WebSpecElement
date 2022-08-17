 /********************************************************************************************************************************************************
 *                                                                                                                                                       *
 *  Project:         Webspector - WebServer based Spectrum Analyzer                                                                                      *
 *  Target Platform: ESP32                                                                                                                               *
 *                                                                                                                                                       * 
 *  Version: 1.0                                                                                                                                         *
 *  Hardware setup: See github                                                                                                                           *
 *                                                                                                                                                       *
 *                                                                                                                                                       * 
 *  Mark Donners                                                                                                                                         *
 *  The Electronic Engineer                                                                                                                              *
 *  Website:   www.theelectronicengineer.nl                                                                                                              *
 *  facebook:  https://www.facebook.com/TheelectronicEngineer                                                                                            *
 *  youtube:   https://www.youtube.com/channel/UCm5wy-2RoXGjG2F9wpDFF3w                                                                                  *
 *  github:    https://github.com/donnersm                                                                                                               *
 *                                                                                                                                                       *  
 ********************************************************************************************************************************************************/
 
/*********************************************************/
int numBands =              64;                     // Default number of bands. change it by pressing the mode button ( 8,16,24,32,64)
#define MODE_BUTTON_PIN     15                      // this is the IO pin you are using for the button
#define GAIN_DAMPEN         2                       // Higher values cause auto gain to react more slowly
int NoiseTresshold =        2000;                   // this will effect the upper bands most.
float Speedfilter=          0.08;                   // slowdown factor for columns to look less 'nervous' The higher the quicker

const int samplingFrequency = 44100;                // The audio sampling frequency. 
const int SAMPLEBLOCK = 1024;                       // size of sample block, only half the size contains useable samples
 

String labels[65];
int BandCutoffTable[65];

// Below are the band frequencies. You can play around with it but be carefull. Also, they have to be sorted from low to high
static int BandCutoffTable8[8] =
{
  100, 250, 500, 1000, 2000, 4000, 8000, 16000
};

String labels8[] =
{
  "100", "250", "500", "1K", "2K", "4K", "8K", "16K"
};

static int BandCutoffTable16[16] =
{
  30, 50, 100, 150, 250, 400, 650, 1000, 1600, 2500, 4000, 6000, 12000, 14000, 16000, 17000
};

String labels16[] =
{
  "30", "50", "100", "150", "250", "400", "650", "1K",
  "1K6", "2K5" , "4K", "6K", "12K", "14K", "16K", "17K"
};

static int BandCutoffTable24[24] =
{
  40, 80, 150, 220, 270, 320, 380, 440, 540, 630,  800, 1000, 1250, 1600, 2000, 2500, 3150,
  3800, 4200, 4800, 5400, 6200, 7400, 12500
};

String labels24[] =
{
  "40", "80", "150", "220", "270", "320", "380", "440",
  "540", "630", " 800", "1000", "1250", "1600", "2000", "2500",
  "3150", "3800", "4200", "4800", "5400", "6200", "7400", "12500"
};

static int BandCutoffTable32[32] =
{
  45, 90, 130, 180, 220, 260, 310, 350,
  390, 440, 480, 525, 650, 825, 1000, 1300,
  1600, 2050, 2500, 3000, 4000, 5125, 6250, 9125,
  12000, 13000, 14000, 15000, 16000, 16500, 17000, 17500
};

String labels32[] =
{
  "45", "90", "130", "180", "220", "260", "310", "350",
  "390", "440", "480", "525", "650", "825", "1K", "1K3",
  "1K6", "2K05", "2K5", "3K", "4K", "5125", "6250", "9125",
  "12K", "13K", "14K", "15K", "16K", "16K5", "17K", "17K5"
};

static int BandCutoffTable64[64] =
{
  45, 90, 130, 180, 220, 260, 310, 350
  , 390, 440, 480, 525, 565, 610, 650, 690
  , 735, 780, 820, 875, 920, 950, 1000, 1050
  , 1080, 1120, 1170, 1210, 1250, 1300, 1340, 1380
  , 1430, 1470, 1510, 1560, 1616, 1767, 1932, 2113
  , 2310, 2526, 2762, 3019, 3301, 3610, 3947, 4315
  , 4718, 5159, 5640, 6167, 6743, 7372, 8061, 8813
  , 9636, 10536, 11520, 12595, 13771, 15057, 16463, 18000

};

String labels64[] =
{
  "45", "90", "130", "180", "220", "260", "310", "350",
  "390", "440", "480", "525", "565", "610", "650", "690",
  "735", "780", "820", "875", "920", "950", "1000", "1050",
  "1080", "1120", "1170", "1210", "1250", "1300", "1340", "1380",
  "1430", "1470", "1510", "1560", "1616", "1767", "1932", "2113",
  "2310", "2526", "2762", "3019", "3301", "3610", "3947", "4315",
  "4718", "5159", "5640", "6167", "6743", "7372", "8061", "8813",
  "9636", "10536", "11520", "12595", "13771", "15057", "16463", "18K"
};

/*********************************************************/

// this is the actual HTML file that will run on the internal server
// Serving a web page (from flash memory)
// formatted as a string literal!
char webpage[] PROGMEM = R"=====(
<html>
<!-- Adding a data chart using Chart.js -->
<head>
  <script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js'></script>

</head>
<body onload="javascript:init()">
<style>

</style>
<h2>Analog Spectrum Analyzer</h2>
<div>
  <canvas id="chart" width="1200" height="800" style="background: url('https://community.element14.com/e14/cfs/e14core/images/logos/e14_Profile_206px.png');  background-repeat: no-repeat;background-position-x: center;;background-position-y: center;"></canvas>
</div>
<!-- Adding a websocket to the client (webpage) -->
<script>
  var webSocket, dataPlot;
  var maxDataPoints = 20;
  const maxValue = 1;//200000000;
  const maxLow = maxValue * 0.6;
  const maxMedium = maxValue * 0.3;
  const maxHigh = maxValue * 0.1;
const image = new Image();
image.src = 'https://www.chartjs.org/img/chartjs-logo.svg';

  function init() {
    webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
    dataPlot = new Chart(document.getElementById("chart"), {
      type: 'bar',
      data: {
        labels: [],        
        datasets: [{
          data: [],
          label: "Low",
          backgroundColor: "green",//"#D6E9C6"
          borderColor:"black",
          borderWidth: 3
        },
        {
          data: [],
          label: "Moderate",
          backgroundColor: "yellow", //"#FAEBCC"
          borderColor:"black",
          borderWidth: 3
        },
        {
          data: [],
          label: "High",
          backgroundColor: "red", //"#EBCCD1"
          borderColor:"black",
          borderWidth: 3
        },
        ]
      }, 
      options: {
          responsive: false,
          animation: false,
          scales: {
              xAxes: [{ stacked: true }],
              yAxes: [{
                  display: true,
                  stacked: true,
                  ticks: {
                    beginAtZero: true,
                    steps: 1000,
                    stepValue: 50,
                    max: maxValue
                  }
              }]
           }
       }
    });

    
    webSocket.onmessage = function(event) {
      var data = JSON.parse(event.data);
      dataPlot.data.labels = [];
      dataPlot.data.datasets[0].data = [];
      dataPlot.data.datasets[1].data = [];
      dataPlot.data.datasets[2].data = [];
      
      data.forEach(function(element) {
        dataPlot.data.labels.push(element.bin);
        var lowValue = Math.min(maxLow, element.value);
        dataPlot.data.datasets[0].data.push(lowValue);
        
        var mediumValue = Math.min(Math.max(0, element.value - lowValue), maxMedium);
        dataPlot.data.datasets[1].data.push(mediumValue);
        
        var highValue = Math.max(0, element.value - lowValue - mediumValue);
        dataPlot.data.datasets[2].data.push(highValue);

      });
      dataPlot.update();
    }
  }

</script>
<table width="100%" border="0" cellspacing="0" cellpadding="0">
  <tr>
    <td><a href="http://www.theelectronicengineer.nl" target="_blank"><h3 align="left">The Electronic Engineer   -   </h3></a></td>
    <td><a href="https://www.buymeacoffee.com/MarkDonners"  target="_blank"><h3 align="left">If you like, you can buy me a coffee to keep me up and running</h3></a></td>
  </tr>
</table>

</body>
</html>
)=====";



// one last array to show info on boot
char Projectinfo[] PROGMEM = R"=====(
ESP32 is connected to Wi-Fi network, Don't forget to reboot if you just reconfigured the network settings.
You can reconfigure WIFI settings by pressing and holding the mode button when rebooting.
Program written by Mark Donners

The Electronic Engineer.
www.theelectronicengineer.nl
email: Mark.Donners@judoles.nl

Needed components:
ESP32 ( obviously )
Connected a switch to pin GPIO15(D15) and ground to create a mode button
Two identical resistor. can be any value between 1K and 50K als long as you use 2 of the same
Connect 1 resistor between pin GPIO36 (vp) and ground. Connect the other resistor to +3.3V and GPIO36 
This will create an offset to your input signal to protect the esp32
Connect a capacitor of 220nF to GPIO36. The other end of the capacitor is your audio line in

             +3.3V
               |                             ________ GPIO15
              | | 10K                       |
              | |                           |
               |                            |
 Audio --||---- ----->GPIO36               \
       220nF   |                            |
              | | 10K                       |
              | |                           |
               |                            |
              gnd                          gnd

The program is running on both cores. Core 1 is used for the main loop and does the FFT analyses
Core 0 is used for the webinterface. Both could run on 1 core but whenever the WIFI signal is disturbed,
for example by putting hand over the ESP32, the program would freeze...and the userinterface is not monitored.
Therefore, the webserver is on a different core. If it freezes up, the user interface will still work.

If you like this program, please buy me a coffee. The link is under the graph in the webinterface
   

)=====";
