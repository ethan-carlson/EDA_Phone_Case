

#include <ICM_20948.h>  
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <PulseSensorAmped.h>

#define AD0_VAL 0  // The value of the last bit of the I2C address. 

/*  PIN ASSIGNMENTS */
#define slp 8
#define eda1_out 5
#define eda2_out 4
#define led_on 7
#define TC1 A1
#define TC2 A2
#define PULSE_SIGNAL_PIN A4

#define IBI_BUFFER_SIZE 5  //HRV will be defined as the standard deviation in IBI over the last 5 beats ***IMPORTANT

// Relevant signals on Rev 2 PCB: 15
// TC1, TP1, TC2, TP2, EDA1, EDA2, Pulse, FSR, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, BrdTemp
// To be added in Rev 3 -- Mic
uint8_t reportTurnCounter = 1;
uint8_t readTurnCounter = 1;
uint8_t edaTurnCounter = 1;
int baseReadCounter = 0;
int tp1ReadCounter = 0;
int tp2ReadCounter = 0;
int fsrReadCounter = 0;
uint8_t ibiBufferCounter = 0;
int avg_tc1, avg_tc2, avg_tp1, avg_tp2, avg_fsr, avg_accx, avg_accy, avg_accz, avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp; //Summary report values (avg)
int max_tc1, max_tc2, max_tp1, max_tp2, max_fsr, max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, max_brdtemp; //Summary report values (max)
int pulse;
float hrv;
unsigned int ibi_buffer[IBI_BUFFER_SIZE];
unsigned long loopTimer, edaReadTimer, reportTimer, otherReadTimer;
String eda1Report = "";
String eda2Report = "";
String summaryReport = "";

int tc1, tc2, tp1, tp2, blood, fsr, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, eda1, eda2;

ICM_20948_I2C ICM;
Adafruit_ADS1115 ads_other(0x48);
Adafruit_ADS1115 ads_eda(0x49);

void setup() {

  //For debugging, comment out when not used
  Serial.begin(115200);
  while(!Serial){}

  Wire.begin();
  Wire.setClock(400000);  //The IMU is capped at the 400khz level
  
  initializeIMU();  // ***This will become blocking when we add the Rev3 sleep code ***

  pinMode(TC1,INPUT);  //Local temp compensation PTC resistors from the thermopile assemblies
  pinMode(TC2,INPUT);
  pinMode(eda1_out,OUTPUT);  //To send voltage out into the hand
  pinMode(eda2_out,OUTPUT);
  pinMode(led_on,OUTPUT);  //To turn the pulse LED on
  pinMode(slp,OUTPUT);  //To turn off much of the board functionality when not in use

  digitalWrite(eda1_out,HIGH);  //On constant for now, later only have this on sometimes to prevent shorts
  digitalWrite(eda2_out,HIGH);
  
  ads_other.setGain(GAIN_TWOTHIRDS);  //Should be able to put this to one eventually
  ads_eda.setGain(GAIN_ONE);  //No higher than one since the max output of the op amp is the same as Vdd on the ADS1115
  ads_other.begin();
  ads_eda.begin();

  digitalWrite(led_on,HIGH);  //For now this is on all the time.  Doesn't seem to work well with the library if partiall off.
  PulseSensorAmped.attach(PULSE_SIGNAL_PIN);
  PulseSensorAmped.start();  //Start reading heart beats

  reportTimer = millis();  //Timer to avoid sending reports more than once per second (Particle cap)
  edaReadTimer = millis();  //Timer to rate limit EDA readings to the Particle publishing size cap (622 bytes)
  otherReadTimer = micros();  //Timer to rate limit the other ADS signals to the max rate of the ADS1115
}

void loop() {
  // Variables handled locally: TC1, TP1, TC2, TP2, Blood, FSR, AccX, AccY, AccZ, GyrX, GyrY, GyrZ, BrdTemp
  // Variables pushed to the cloud raw: EDA1, EDA2
  long loopTimer;
  loopTimer = millis();

  /*  Cloud Variables */
  // 100 6-byte readings = 600 byte reports
  // Report every three seconds --> readings every 30ms
  if((edaTurnCounter == 1) && ((millis()-edaReadTimer) >= 15)){
    edaReadTimer = millis();
    eda1 = ads_eda.readADC_SingleEnded(0);
    eda1Report = eda1Report + String(eda1) + ','; //EDA1
    edaTurnCounter = 2;
  }
  
  //  Can get 20ms resolution instead of 30ms by dropping the EDA2 circuit...evaluate its usefulness in Rev3
  else if((edaTurnCounter == 2) && ((millis()-edaReadTimer) >= 15)){
    edaReadTimer = millis();
    eda2 = ads_eda.readADC_SingleEnded(2);
    eda2Report = eda2Report + String(eda2) + ','; //EDA2
    edaTurnCounter = 1;
  }

  /*  Locally Used Variables  */
  tc1 = analogRead(TC1);
  tc2 = analogRead(TC2);
  ICM.getAGMT();  // Update IMU Values
  accx = ICM.agmt.acc.axes.x;
  accy = ICM.agmt.acc.axes.y;
  accz = ICM.agmt.acc.axes.z;
  gyrx = ICM.agmt.gyr.axes.x;
  gyry = ICM.agmt.gyr.axes.y;
  gyrz = ICM.agmt.gyr.axes.z;
  brdtemp = ICM.agmt.tmp.val;

  int summaryArray[9] = {tc1, tc2, accx, accy, accz, gyrx, gyry, gyrz, brdtemp};
  baseReadCounter++;
  updateSummaryVals(summaryArray);

  PulseSensorAmped.process();


  //The ADS1115 can read at 860 SPS, roughly every 1160 us
  if((micros() - otherReadTimer) > 1500){  //This is probably superfluous given our current loop time of ~5-10 ms
    otherReadTimer = micros();
    switch (readTurnCounter){ //Take turns reading from each line
      case 1:
        tp1 = ads_other.readADC_SingleEnded(0);  //Thermopile #1
        avg_tp1 = avg_tp1 + tp1;
        if(tp1 > max_tp1){
          max_tp1 = tp1;
        }
        tp1ReadCounter++;
        readTurnCounter = 2;
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
  if((millis() - reportTimer) >= 1001){
    reportTimer = millis();
    switch (reportTurnCounter){
      case 1:
        if(Particle.connected()){
          Particle.publish("EDA1", eda1Report, PRIVATE);
        }
        eda1Report = "";
        reportTurnCounter = 2;
        break;
      case 2:
        if(Particle.connected()){
          Particle.publish("EDA2", eda2Report, PRIVATE);
        }
        eda2Report = "";
        reportTurnCounter = 3;
        break;
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
  // Serial.println(tp1);
  // Serial.print("TP2: ");
  // Serial.println(tp2);
  // Serial.print("FSR: ");
  // Serial.println(fsr);
  // Serial.print("AccZ: ");
  // // Serial.println(accz);
  // Serial.print("Pulse BPM: ");
  // Serial.println(pulse);
  // Serial.print("HRV: ");
  // Serial.println(hrv);

  // Serial.print("Loop Time: ");
  // Serial.println((millis() - loopTimer));
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
  Serial.println("Pulse Lost");
}


void computeSummaryReport(){
  avg_tc1 = avg_tc1 / baseReadCounter;
  avg_tc2 = avg_tc2 / baseReadCounter;
  avg_accx = avg_accx / baseReadCounter;
  avg_accy = avg_accy / baseReadCounter;
  avg_accz = avg_accz / baseReadCounter;
  avg_gyrx = avg_gyrx / baseReadCounter;
  avg_gyry = avg_gyry / baseReadCounter;
  avg_gyrz = avg_gyrz / baseReadCounter;
  avg_brdtemp = avg_brdtemp / baseReadCounter;
  avg_tp1 = avg_tp1 / tp1ReadCounter;
  avg_tp2 = avg_tp2 / tp2ReadCounter;
  avg_fsr = avg_fsr / fsrReadCounter;

  float voltage = analogRead(BATT) * 0.0011224;
  float batt_perc = ((voltage - 3.0) / 1.2) * 100.0;

  summaryReport = String::format("{ \"AvgTC1\": %d, \"AvgTP1\": %d, \"AvgTC2\": %d, \"AvgTP2\": %d, \"AvgFSR\": %d \
                                  , \"AvgAccX\": %d, \"AvgAccY\": %d, \"AvgAccZ\": %d, \"AvgGyrX\": %d, \"AvgGyrY\": %d \
                                  , \"AvgGyrZ\": %d, \"AvgBT\": %d, \"MaxTC1\": %d, \"MaxTP1\": %d, \"MaxTC2\": %d \
                                  , \"MaxTP2\": %d, \"MaxAccX\": %d, \"MaxAccY\": %d, \"MaxAccZ\": %d, \"MaxGyrX\": %d \
                                  , \"MaxGyrY\": %d, \"MaxGyrZ\": %d, \"MaxBT\": %d, \"HR\": %d, \"HRV\": %f \
                                  , \"Batt\": %f, \"MaxFSR\": %d}" \
                                  , avg_tc1, avg_tp1, avg_tc2, avg_tp2, avg_fsr, avg_accx, avg_accy, avg_accz, \
                                  avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp, max_tc1, max_tp1, max_tc2, max_tp2, \
                                  max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, max_brdtemp, pulse, \
                                  hrv, batt_perc, max_fsr);

Serial.println(avg_tc1);
Serial.println(avg_tp1);
Serial.println(avg_tc2);
Serial.println(avg_tp2);
Serial.print("Avg_FSR: ");
Serial.println(avg_fsr);
Serial.println(avg_accx);
Serial.println(avg_accy);
Serial.println(avg_accz);
Serial.println(avg_gyrx);
Serial.println(avg_gyry);
Serial.println(avg_gyrz);
Serial.print("Avg_BrdTemp: ");
Serial.println(avg_brdtemp);
Serial.println(max_tc1);
Serial.println(max_tp1);
Serial.println(max_tc2);
Serial.println(max_tp2);
//Good up through the above
Serial.print("Max_FSR: ");
Serial.println(max_fsr);
Serial.println(max_accx);
Serial.println(max_accy);
Serial.println(max_accz);
Serial.print("Max_GyrX: ");
Serial.println(max_gyrx);
Serial.println(max_gyry);
Serial.println(max_gyrz);
Serial.println(max_brdtemp);
Serial.print("Pulse: ");
Serial.println(pulse);
Serial.print("HRV: ");
Serial.println(hrv);
Serial.print("Batt: ");
Serial.println(batt_perc);
Serial.println(" ");

  baseReadCounter = 0;
  tp1ReadCounter = 0;
  tp2ReadCounter = 0;
  fsrReadCounter = 0;
}


void updateSummaryVals(int summaryArray[9]){
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