//#define USE_ARDUINO_INTERRUPTS false
//#include <PulseSensorPlayground.h>

#include <string.h>
#include <ICM_20948.h>  
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <WiFi.h>
#include <HTTPClient.h>

TaskHandle_t Core0;  //Set up the handle for sensor tasks which will run on Core 0

/*  WiFi and Server Reporting Info  */
const char* ssid = "WSMCP?";
const char* password = "phoebecat";
const char* edaServer = "https://www.ethan-carlson.com/eda_update.php";
const char* summaryServer = "https://www.ethan-carlson.com/phone_case_update.php";


#define AD0_VAL 0  // The value of the last bit of the IMU I2C address. 

/*  PIN ASSIGNMENTS */
#define eda_depol 32
#define eda_out 15
#define alrt 33
#define slp 13
#define led_on 26
#define ST 25
#define FSR 34
#define MIC 39
#define PULSE 36
#define BATT 35

#define IBI_BUFFER_SIZE 8  //HRV will be defined as the standard deviation in IBI over the last 8 beats ***IMPORTANT

// Relevant signals on Rev 4 PCB: 16
// SkinTemp, EDA, Pulse, FSR, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, BrdTemp, Mic
uint8_t reportTurnCounter = 1;
int baseReadCounter = 1;
uint8_t ibiBufferCounter = 0;
int avg_st, avg_fsr, avg_accx, avg_accy, avg_accz, avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp, avg_mic; //Summary report values (avg)
int max_st, max_fsr, max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, max_brdtemp, max_mic; //Summary report values (max)
int pulse;
float hrv;
unsigned int ibi_buffer[IBI_BUFFER_SIZE];
long loopTimer, edaReadTimer, reportTimer, sleepTimer;
char edaReport[5000] = {0};
char timestring[2500] = {0};
char summaryReport[500] = {0};

int skintemp, fsr, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, eda, mic;

ICM_20948_I2C ICM;
Adafruit_ADS1115 ads_eda(0x49);

/*
const int OUTPUT_TYPE = SERIAL_PLOTTER;
const int THRESHOLD = 2400;   // Adjust this number to avoid noise when idle
byte samplesUntilReport;
const byte SAMPLES_PER_SERIAL_SAMPLE = 10;
PulseSensorPlayground pulseSensor;
*/

volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded! 
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

/*-----------------------------------------------------------------------------------------------*/

void setup() {

  
  //For debugging, comment out when not used
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000);  //The IMU is capped at the 400khz level
  
  pinMode(ST,INPUT);  //Skin temp NTC thermistor
  pinMode(FSR,INPUT);  //Grasping force sensitive resistor
  pinMode(alrt, INPUT);  //Device turns off when there's no EDA activity
  pinMode(MIC, INPUT);  //Measure ambient noise level
  pinMode(eda_out,OUTPUT);  //To send voltage out into the hand
  pinMode(led_on,OUTPUT);  //To turn the pulse LED on
  pinMode(slp,OUTPUT);  //To turn off much of the board functionality when not in use
  pinMode(eda_depol, OUTPUT); // To depolarize the eda electrodes.  Not implemented in Rev3 due to PCB error.

  digitalWrite(eda_depol,LOW);  //Pull low to start
  digitalWrite(eda_out,HIGH);  //On constant for now, later only have this on sometimes to prevent shorts

  digitalWrite(slp, HIGH);  //Start with board awake
  delay(100);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

//  initializeIMU();
  ICM.begin( Wire, AD0_VAL );

  ads_eda.setGain(GAIN_ONE);  
  ads_eda.begin();
  
  xTaskCreatePinnedToCore(
                    core0Tasks,   /* Task function. */
                    "Core0",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */ 


  digitalWrite(led_on,HIGH);  //On constant for now
  interruptSetup();  // sets up to read Pulse Sensor signal every 2mS 
  /*
  pulseSensor.analogInput(PULSE);
  pulseSensor.setSerial(Serial);
  pulseSensor.setOutputType(OUTPUT_TYPE);
  pulseSensor.setThreshold(THRESHOLD);
  samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
  pulseSensor.begin();
*/
  delay(100);

  reportTimer = millis();  //Timer to limit number of HTTP requests
  edaReadTimer = millis();  //Timer to rate limit EDA readings to avoid overtaxing the ADS1115
  sleepTimer = millis();  //Timer to figure out when the device isn't being used and go to sleep
}

/*-----------------------------------------------------------------------------------------------*/

void loop() {
  //Publish a report every second
  if((millis() - reportTimer) >= 2000){
    reportTimer = millis();
    switch (reportTurnCounter){
      case 1:
        edaHTTP();
        memset(edaReport, 0, sizeof(edaReport));
        sprintf(edaReport, "EDAstring=");
        memset(timestring, 0, sizeof(timestring));
        sprintf(timestring, "&timestring=");
        reportTurnCounter = 2;
        break;
      case 2:
        SummaryHTTP();
        reportTurnCounter = 1;
        break;
    }
  }
}

/*-----------------------------------------------------------------------------------------------*/

void checkRollover(){
  //Deal with millis() rollover every 49.7 days
  if (((millis() - reportTimer) < 0) || ((millis() - edaReadTimer) < 0) || ((millis() - sleepTimer) < 0)){
    Serial.println("Rollover Trigger");
    reportTimer = millis();
    edaReadTimer = millis();
    sleepTimer = millis();
  }
}

/*-----------------------------------------------------------------------------------------------*/

void initializeIMU(){
  bool initialized = false;
  while( !initialized ){

    ICM.begin( Wire, AD0_VAL );

    Serial.print( F("Initialization of the sensor returned: ") );
    Serial.println( ICM.statusString() );
    if( ICM.status != ICM_20948_Stat_Ok ){
      Serial.println( "Trying again..." );
      delay(500);
    }else{
      initialized = true;
    }
  }
}

/*-----------------------------------------------------------------------------------------------*/
/*
void gotoSleep(){
  bool sleeping = true;

  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::STOP)  //Using the STOP configuration leaves the GPIO active and variable states intact
        .gpio(alrt, FALLING)
        .duration(10min);  //Wake up every 1 sec to check

  PulseSensorAmped.stop();  //Stop reading heart beats 
  digitalWrite(slp, LOW);  //Turn most of the board off
  digitalWrite(led_on, LOW);  //Turn the pulse sensor off
  ads_eda.startComparator_SingleEnded(0,100);  //Set up the alrt pin to notify when the device is being used

  while(sleeping){
    bool alrt_flag;
    
    System.sleep(config);  //Put the Argon to sleep

    //On wakeup, continue here:
    if((System.millis() - reportTimer) >= 1001){  //If it's safe to do so, shoot off a battery report
      reportTimer = System.millis();
      computeSummaryReport();
      if(Particle.connected()){
        Particle.publish("SummaryReport", summaryReport, PRIVATE);
      }
      summaryReport = "";
    }
    
    alrt_flag = digitalRead(alrt);  //Check to see if we're  awake
    if (!alrt_flag){  
      sleeping = false;  //If so leave the loop
    }
  }

  if(!sleeping){
    digitalWrite(slp, HIGH);  //Turn the board back on
    digitalWrite(led_on, HIGH);  //Turn the pulse sensor back on
    delay(100);  //Let it wake up
    initializeIMU();
    ads_other.begin();
    ads_eda.begin();
    PulseSensorAmped.start();  //Start reading heart beats

    sleepTimer = System.millis();  //Reset sleep timer

    return; //head back to main
  }
}
*/
