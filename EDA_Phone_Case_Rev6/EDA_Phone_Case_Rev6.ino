#include <string.h>
#include <ICM_20948.h>  
#include <Wire.h>
#include <HTTPClient.h>
#include "Adafruit_DRV2605.h"

bool biofeedback = false;  // Set true to engage haptic heart rate biofeedback

TaskHandle_t Core0;  //Set up the handle for sensor tasks which will run on Core 0

/*  WiFi and Server Reporting Info  */
const char* ssid = "WSMCP?";
const char* password = "phoebecat";
const char* edaServer = "https://www.ethan-carlson.com/eda_update.php";
const char* summaryServer = "https://www.ethan-carlson.com/phone_case_update.php";

#define AD0_VAL 0  // The value of the last bit of the IMU I2C address. 

/*  PIN ASSIGNMENTS */
#define eda_out 15
#define eda_depol 14
#define brd_led 13
#define led_on 26
#define EDA 33
#define ST 32
#define FSR 34
#define MIC 39
#define PULSE 36
#define BATT 35

/*  Relevant signals on Rev 6 PCB: 12  */
// SkinTemp, EDA, HR, HRV, AvgFSR, MaxFSR, AvgAcc, AvgGyr, MaxAcc, MaxGyr, BrdTemp, MicRng
uint8_t reportTurnCounter = 1;
int baseReadCounter = 1;
int avg_st, avg_fsr, avg_acc, avg_gyr, avg_brdtemp, min_mic; //Summary report values (avg)
int max_st, max_fsr, max_acc, max_gyr, max_mic; //Summary report values (max)
float mag_acc, mag_gyr;
long loopTimer, edaReadTimer, reportTimer, sleepTimer, haptic_pulse_to, start_time, tapTimer;
char edaRequest[5000] = {0};
char edaReport[2500] = {0};
char timestring[2500] = {0};
char summaryReport[500] = {0};
bool firstTap = false;
bool secondTap = false;
int skintemp, fsr, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, eda, mic;

ICM_20948_I2C ICM;
Adafruit_DRV2605 drv;

float hrv, hrv_calc;                // used to calculate HRV as the std dev of the las 10 IBI vals
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded! 
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low

/*-----------------------------------------------------------------------------------------------*/

void setup() {
  
  //For debugging, comment out when not used
  Serial.begin(115200);

  /*  Pin Mode Declarations  */
  pinMode(ST,INPUT);          //Skin temp NTC thermistor
  pinMode(FSR,INPUT);         //Grasping force sensitive resistor
  pinMode(EDA,INPUT);         //EDA ADC input
  pinMode(MIC, INPUT);        //Measure ambient noise level
  pinMode(eda_out,OUTPUT);    //To send voltage out into the hand
  pinMode(led_on,OUTPUT);     //To turn the pulse LED on
  pinMode(eda_depol, OUTPUT); //To depolarize the eda electrodes.
  pinMode(brd_led, OUTPUT);   //Control for on board indicator light to show wakeup

  /*  Start Electrode Depolarization Cycle on restart  */
  digitalWrite(eda_out,LOW);     //Pull the EDA pin low to reduce current flow (also protected by PTC fuse)
  digitalWrite(eda_depol,LOW);   //Pull LOW to start electrode depolarization cycle

  Wire.begin();
  Wire.setClock(400000);    //The IMU is capped at the 400khz level

  /*  Flash to indicate that a reset has occured  */
  for(int i=0;i<5;i++){
    digitalWrite(brd_led, HIGH);
    delay(100);
    digitalWrite(brd_led, LOW);
    delay(100);
  }

  /*  Connect to WiFi  */
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  /*  Attach the sensor functions to core 0  */
  xTaskCreatePinnedToCore(
                    core0Tasks,  /* Task function. */
                    "Core0",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */ 

  delay(500);

  /*  End Electrode Depolarization  */
  digitalWrite(eda_depol,HIGH);   //Pull HIGH to turn off depolarization
  digitalWrite(eda_out,HIGH);     //Set the EDA output to be on

  reportTimer = millis();  //Timer to limit number of HTTP requests
  edaReadTimer = millis();  //Timer to rate limit EDA readings
  sleepTimer = millis();  //Timer to figure out when the device isn't being used and go to sleep
  haptic_pulse_to = millis();  //Timout so we don't trigger haptic feedback more than once pur heartbeat
  start_time = millis();  //Prevent things from happening in the first 10 seconds
  
}

/*-----------------------------------------------------------------------------------------------*/

void loop() {
  //Publish a report every two seconds
  if((millis() - reportTimer) >= 2000){
    reportTimer = millis();
    switch (reportTurnCounter){
      case 1:
        edaHTTP();
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
