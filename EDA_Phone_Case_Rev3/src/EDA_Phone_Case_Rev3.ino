
#include <ICM_20948.h>  
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <PulseSensorAmped.h>

#define AD0_VAL 0  // The value of the last bit of the I2C address. 

/*  PIN ASSIGNMENTS */
#define eda1_depol 2
#define eda2_depol 3
#define eda2_out 4
#define eda1_out 5
#define alert 6
#define slp 8
#define led_on A0
#define TC1 A1
#define TC2 A2
#define MIC A3
#define PULSE_SIGNAL_PIN A4

#define IBI_BUFFER_SIZE 8  //HRV will be defined as the standard deviation in IBI over the last 8 beats ***IMPORTANT

// Relevant signals on Rev 3 PCB: 16
// TC1, TP1, TC2, TP2, EDA1, EDA2, Pulse, FSR, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, BrdTemp, Mic
uint8_t reportTurnCounter = 1;
uint8_t readTurnCounter = 1;
uint8_t edaTurnCounter = 1;
int baseReadCounter = 1;
int tp1ReadCounter = 1;
int tp2ReadCounter = 1;
int fsrReadCounter = 1;
uint8_t ibiBufferCounter = 0;
int avg_tc1, avg_tc2, avg_tp1, avg_tp2, avg_fsr, avg_accx, avg_accy, avg_accz, avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp, avg_mic; //Summary report values (avg)
int max_tc1, max_tc2, max_tp1, max_tp2, max_fsr, max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, max_brdtemp, max_mic; //Summary report values (max)
int pulse;
float hrv;
unsigned int ibi_buffer[IBI_BUFFER_SIZE];
unsigned long long loopTimer, edaReadTimer, reportTimer, otherReadTimer, sleepTimer;
String eda1Report = "";
String eda2Report = "";
String summaryReport = "";

int tc1, tc2, tp1, tp2, blood, fsr, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, eda1, eda2, mic;

ICM_20948_I2C ICM;
Adafruit_ADS1115 ads_other(0x48);
Adafruit_ADS1115 ads_eda(0x49);

void setup() {

  //For debugging, comment out when not used
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000);  //The IMU is capped at the 400khz level
  
  pinMode(TC1,INPUT);  //Local temp compensation NTC thermistors from the thermopile assemblies
  pinMode(TC2,INPUT);
  pinMode(alert, INPUT);  //Device turns off when there's no EDA activity
  pinMode(MIC, INPUT);  //Measure ambient noise level
  pinMode(eda1_out,OUTPUT);  //To send voltage out into the hand
  pinMode(eda2_out,OUTPUT);
  pinMode(led_on,OUTPUT);  //To turn the pulse LED on
  pinMode(slp,OUTPUT);  //To turn off much of the board functionality when not in use
  pinMode(eda1_depol, OUTPUT); // To depolarize the eda1 electrodes.  Not implemented in Rev3 due to PCB error.
  pinMode(eda2_depol, OUTPUT);

  digitalWrite(eda1_depol,LOW);  //Not implemented in Rev3 so pull low for safety
  digitalWrite(eda2_depol,LOW);
  digitalWrite(eda1_out,HIGH);  //On constant for now, later only have this on sometimes to prevent shorts
  digitalWrite(eda2_out,HIGH);

  digitalWrite(slp, HIGH);  //Start with board awake
  delay(100);
  
  initializeIMU();

  ads_other.setGain(GAIN_TWOTHIRDS);  
  ads_eda.setGain(GAIN_ONE);  
  ads_other.begin();
  ads_eda.begin();

  digitalWrite(led_on,HIGH);  //For now this is on all the time.  Doesn't seem to work well with the library if partially off.
  PulseSensorAmped.attach(PULSE_SIGNAL_PIN);
  PulseSensorAmped.start();  //Start reading heart beats

  delay(100);

  reportTimer = System.millis();  //Timer to avoid sending reports more than once per second (Particle cap)
  edaReadTimer = System.millis();  //Timer to rate limit EDA readings to the Particle publishing size cap (622 bytes)
  otherReadTimer = micros();  //Timer to rate limit the other ADS signals to the max rate of the ADS1115
  sleepTimer = System.millis();  //Timer to figure out when the device isn't being used and go to sleep
}

void loop() {
  // Variables handled locally: TC1, TP1, TC2, TP2, Blood, FSR, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, BrdTemp, Mic
  // Variables pushed to the cloud raw: EDA1, EDA2

  //long loopTimer;
  //loopTimer = System.millis();

  if ((System.millis() - sleepTimer) > 15000){  //Go to sleep after 10s of non-use
    gotoSleep();
  }

  /*  Cloud Variables */
  // 100 6-byte readings = 600 byte reports
  // Report every two seconds --> readings every 20ms
  if((edaTurnCounter == 1) && ((System.millis()-edaReadTimer) >= 20)){
    edaReadTimer = System.millis();
    eda1 = ads_eda.readADC_Differential_0_1();
    eda1Report = eda1Report + String(eda1) + ' '; //EDA1

    if(eda1 > 100){  //If the device is being held, reset the sleep timer
      sleepTimer = System.millis();
    }

    //edaTurnCounter = 1;
  }
  /*
  //  Can get 20ms resolution instead of 30ms by dropping the EDA2 circuit...evaluate its usefulness in Rev3
  else if((edaTurnCounter == 2) && ((System.millis()-edaReadTimer) >= 15)){
    edaReadTimer = System.millis();
    eda2 = ads_eda.readADC_SingleEnded(2);
    eda2Report = eda2Report + String(eda2) + ' '; //EDA2
    edaTurnCounter = 1;
  }
*/
  /*  Locally Used Variables  */
  tc1 = analogRead(TC1);
  tc2 = analogRead(TC2);
  ICM.getAGMT();  // Update IMU Values
  accx = ICM.accX()*100;
  accy = ICM.accY()*100;
  accz = ICM.accZ()*100;
  gyrx = ICM.gyrX()*100;
  gyry = ICM.gyrY()*100;
  gyrz = ICM.gyrZ()*100;
  brdtemp = ICM.temp()*100;
  mic = analogRead(MIC);

  int summaryArray[10] = {tc1, tc2, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, mic};
  baseReadCounter++;
  updateSummaryVals(summaryArray);

  PulseSensorAmped.process();

  //ADS1115 currently set for 250 SPS = 4ms min read time
  if((micros() - otherReadTimer) > 4100){  //This is probably superfluous given our current loop time of ~5-10 ms
    otherReadTimer = micros();
    switch (readTurnCounter){ //Take turns reading from each line
      case 1:
        tp1 = ads_other.readADC_SingleEnded(0);  //Thermopile #1
        avg_tp1 = avg_tp1 + tp1;
        if(tp1 > max_tp1){
          max_tp1 = tp1;
        }
        tp1ReadCounter++;
        readTurnCounter = 3;
        break;
      case 2:
        tp2 = ads_other.readADC_SingleEnded(1);  //Thermopile #2
        avg_tp2 = avg_tp2 + tp2;
        if(tp2 > max_tp2){
          max_tp2 = tp2;
        }
        tp2ReadCounter++;
        readTurnCounter = 3;
        break;
      case 3:
        fsr = ads_other.readADC_SingleEnded(2); //FSR
        avg_fsr = avg_fsr + fsr;
        if(fsr > max_fsr){
          max_fsr = fsr;
        }
        fsrReadCounter++;
        readTurnCounter = 1;
        break;
    }
  }

  //Publish a report every second -- Rate capped by Particle
  if((System.millis() - reportTimer) >= 1001){
    reportTimer = System.millis();
    switch (reportTurnCounter){
      case 1:
        if(Particle.connected()){
          eda1Report = eda1Report + "\"}";
          Particle.publish("EDA1", eda1Report, PRIVATE);
        }
        eda1Report = "{ \"EDA1\": \"";
        reportTurnCounter = 3;
        break;
        /*
      case 2:
        if(Particle.connected()){
          eda2Report = eda2Report + "\"}";
          Particle.publish("EDA2", eda2Report, PRIVATE);
        }
        eda2Report = "{ \"EDA2\": \"";
        reportTurnCounter = 3;
        break;
        */
      case 3:
        computeSummaryReport();
        if(Particle.connected()){
          Particle.publish("SummaryReport", summaryReport, PRIVATE);
        }
        summaryReport = "";
        reportTurnCounter = 1;
        break;
    }
  }

  // Serial.print("EDA1: ");
  // Serial.println(eda1);
  // Serial.print("EDA2: ");
  // Serial.println(eda2);
  // Serial.print("TP1: ");
  // Serial.print(tp1);
  // Serial.print(", TC1: ");
  // Serial.print(tc1);
  // Serial.print("TP2: ");
  // Serial.println(tp2);
  // Serial.print(", FSR: ");
  // Serial.println(fsr);
  // Serial.print("AccZ: ");
  // // Serial.println(accz);
  // Serial.print("Pulse BPM: ");
  // Serial.println(pulse);
  // Serial.print("HRV: ");
  // Serial.println(hrv);

  //Deal with micros() rollover every 70min
  if ((micros() - otherReadTimer) < 0){
    otherReadTimer = micros();
  }

   //Serial.print("Loop Time: ");
   //Serial.println((System.millis() - loopTimer));
}


void PulseSensorAmped_data(int BPM, int IBI){  //This is clunky.  Could clean up later
  pulse = BPM;
}


void calculateHRV(int IBI){
  float avg_ibi = 0;
  float ibi_stdev = 0;

  /*  Add the latest IBI to the buffer */
  ibi_buffer[ibiBufferCounter] = IBI;
  ibiBufferCounter++;
  if (ibiBufferCounter >= IBI_BUFFER_SIZE){
    ibiBufferCounter = 0;
  }

  /*  Compute the Standard Deviation  */
  for (int i=0;i<IBI_BUFFER_SIZE;i++){
    avg_ibi += ibi_buffer[i]/IBI_BUFFER_SIZE;
  }
  for (int j=0;j<IBI_BUFFER_SIZE;j++){
    ibi_stdev += pow((ibi_buffer[j] - avg_ibi), 2);
  }
  ibi_stdev /= IBI_BUFFER_SIZE;
  ibi_stdev = pow(ibi_stdev, 0.5);

  hrv = ibi_stdev;
}


void PulseSensorAmped_lost(void) {
  //Serial.println("Pulse Lost");
}


void computeSummaryReport(){
  avg_tc1 = avg_tc1 / (baseReadCounter - 1);
  avg_tc2 = avg_tc2 / (baseReadCounter - 1);
  avg_accx = avg_accx / (baseReadCounter - 1);
  avg_accy = avg_accy / (baseReadCounter - 1);
  avg_accz = avg_accz / (baseReadCounter - 1);
  avg_gyrx = avg_gyrx / (baseReadCounter - 1);
  avg_gyry = avg_gyry / (baseReadCounter - 1);
  avg_gyrz = avg_gyrz / (baseReadCounter - 1);
  avg_brdtemp = avg_brdtemp / (baseReadCounter - 1);
  avg_mic = avg_mic / (baseReadCounter - 1);
  avg_tp1 = avg_tp1 / (tp1ReadCounter - 1);
  avg_tp2 = avg_tp2 / (tp2ReadCounter - 1);
  avg_fsr = avg_fsr / (fsrReadCounter - 1);

  float voltage = analogRead(BATT) * 0.0011224;
  float batt_perc = ((voltage - 3.0) / 1.125) * 100.0;

  summaryReport = String::format("{ \"ATC1\": %d, \"ATP1\": %d, \"ATC2\": %d, \"ATP2\": %d, \"AFSR\": %d \
                                  , \"AAccX\": %d, \"AAccY\": %d, \"AAccZ\": %d, \"AGyrX\": %d, \"AGyrY\": %d \
                                  , \"AGyrZ\": %d, \"ABT\": %d, \"AMic\": %d, \"MTP1\": %d \
                                  , \"MTP2\": %d, \"MAccX\": %d, \"MAccY\": %d, \"MAccZ\": %d, \"MGyrX\": %d \
                                  , \"MGyrY\": %d, \"MGyrZ\": %d, \"HR\": %d, \"HRV\": %.1f \
                                  , \"Batt\": %.1f, \"MFSR\": %d, \"MMic\": %d}" \
                                  , avg_tc1, avg_tp1, avg_tc2, avg_tp2, avg_fsr, avg_accx, avg_accy, avg_accz, \
                                  avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp, avg_mic, max_tp1, max_tp2, \
                                  max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, pulse, \
                                  hrv, batt_perc, max_fsr, max_mic);

  max_tp1 = 0;
  max_tp2 = 0;
  max_accx = 0;
  max_accy = 0;
  max_accz = 0;
  max_gyrx = 0;
  max_gyry = 0;
  max_gyrz = 0;
  max_fsr = 0;
  max_mic = 0;

  baseReadCounter = 0;
  tp1ReadCounter = 0;
  tp2ReadCounter = 0;
  fsrReadCounter = 0;
}


void updateSummaryVals(int summaryArray[10]){
  avg_tc1 = avg_tc1 + summaryArray[0];
  if(summaryArray[0] > max_tc1){
    max_tc1 = summaryArray[0];
  }
  avg_tc2 = avg_tc2 + summaryArray[1];
  if(summaryArray[1] > max_tc2){
    max_tc2 = summaryArray[1];
  }
  avg_accx = avg_accx + summaryArray[2];
  if(summaryArray[2] > max_accx){
    max_accx = summaryArray[2];
  }
  avg_accy = avg_accy + summaryArray[3];
  if(summaryArray[3] > max_accy){
    max_accy = summaryArray[3];
  }
  avg_accz = avg_accz + summaryArray[4];
  if(summaryArray[4] > max_accz){
    max_accz = summaryArray[4];
  }
  avg_gyrx = avg_gyrx + summaryArray[5];
  if(summaryArray[5] > max_gyrx){
    max_gyrx = summaryArray[5];
  }
  avg_gyry = avg_gyry + summaryArray[6];
  if(summaryArray[6] > max_gyry){
    max_gyry = summaryArray[6];
  }
  avg_gyrz = avg_gyrz + summaryArray[7];
  if(summaryArray[7] > max_gyrz){
    max_gyrz = summaryArray[7];
  }
  avg_brdtemp = avg_brdtemp + summaryArray[8];
  if(summaryArray[8] > max_brdtemp){
    max_brdtemp = summaryArray[8];
  }
  avg_mic = avg_mic + summaryArray[9];
  if(summaryArray[9] > max_mic){
    max_mic = summaryArray[9];
  }
}


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

void gotoSleep(){
  bool sleeping = true;

  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::STOP)  //Using the STOP configuration leaves the GPIO active and variable states intact
        .gpio(alert, FALLING)
        .duration(10min);  //Wake up every 1 sec to check

  PulseSensorAmped.stop();  //Stop reading heart beats 
  digitalWrite(slp, LOW);  //Turn most of the board off
  digitalWrite(led_on, LOW);  //Turn the pulse sensor off
  ads_eda.startComparator_SingleEnded(0,100);  //Set up the alert pin to notify when the device is being used

  while(sleeping){
    bool alert_flag;
    
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
    
    alert_flag = digitalRead(alert);  //Check to see if we're  awake
    if (!alert_flag){  
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