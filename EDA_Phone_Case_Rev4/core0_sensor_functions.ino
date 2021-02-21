void core0Tasks(void * parameter){
  
  for(;;){  //Run continuously
  //  long loopTimer;
  //  loopTimer = millis();
  
    if ((millis() - sleepTimer) > 20000){  //Go to sleep after 20s of non-use
      //gotoSleep();
    }
  

    if((millis()-edaReadTimer) >= 10){  //Read every ~10ms.
      int timediff = millis()-edaReadTimer;
      edaReadTimer = millis();
      eda = ads_eda.readADC_Differential_0_1();
      char edaRead[10] = {0};
      sprintf(edaRead,"%d ", eda);
      strcat(edaReport, edaRead);
      char timeRead[10] = {0};
      sprintf(timeRead,"%d ", timediff);
      strcat(timestring, timeRead);
      if(eda > 100){  //If the device is being held, reset the sleep timer
        sleepTimer = millis();
      }
    }
    
    ICM.getAGMT();  // Update IMU Values
    accx = ICM.accX()*100;
    accy = ICM.accY()*100;
    accz = ICM.accZ()*100;
    gyrx = ICM.gyrX()*100;
    gyry = ICM.gyrY()*100;
    gyrz = ICM.gyrZ()*100;
    brdtemp = ICM.temp()*100;

    skintemp = analogRead(ST);
    mic = analogRead(MIC);
    fsr = analogRead(FSR);
/*
    pulseSensor.sawNewSample();
    calculateHRV(pulseSensor.getInterBeatIntervalMs());
*/
    int summaryArray[10] = {skintemp, accx, accy, accz, gyrx, gyry, gyrz, brdtemp, mic, fsr};
    baseReadCounter++;
    updateSummaryVals(summaryArray);


    checkRollover();
    
    //Serial.print("Sesnor Loop Time: ");
    //Serial.println((millis() - loopTimer));
    yield();
  }
}

/*-----------------------------------------------------------------------------------------------*/

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

/*-----------------------------------------------------------------------------------------------*/

void updateSummaryVals(int summaryArray[10]){
  avg_st = avg_st + summaryArray[0];
  if(summaryArray[0] > max_st){
    max_st = summaryArray[0];
  }
  avg_accx = avg_accx + summaryArray[1];
  if(summaryArray[1] > max_accx){
    max_accx = summaryArray[2];
  }
  avg_accy = avg_accy + summaryArray[2];
  if(summaryArray[2] > max_accy){
    max_accy = summaryArray[3];
  }
  avg_accz = avg_accz + summaryArray[3];
  if(summaryArray[3] > max_accz){
    max_accz = summaryArray[4];
  }
  avg_gyrx = avg_gyrx + summaryArray[4];
  if(summaryArray[4] > max_gyrx){
    max_gyrx = summaryArray[5];
  }
  avg_gyry = avg_gyry + summaryArray[5];
  if(summaryArray[5] > max_gyry){
    max_gyry = summaryArray[6];
  }
  avg_gyrz = avg_gyrz + summaryArray[6];
  if(summaryArray[6] > max_gyrz){
    max_gyrz = summaryArray[7];
  }
  avg_brdtemp = avg_brdtemp + summaryArray[7];
  if(summaryArray[7] > max_brdtemp){
    max_brdtemp = summaryArray[8];
  }
  avg_mic = avg_mic + summaryArray[8];
  if(summaryArray[8] > max_mic){
    max_mic = summaryArray[9];
  }
  avg_fsr = avg_fsr + summaryArray[9];
  if(summaryArray[9] > max_fsr){
    max_fsr = summaryArray[10];
  }
}

/*-----------------------------------------------------------------------------------------------*/
