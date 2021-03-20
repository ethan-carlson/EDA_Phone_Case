void core0Tasks(void * parameter){

  initializeIMU();
  
  drv.begin();
  drv.selectLibrary(1);

  // The interrupt setup especially has to stay here so the ISR runs on the right core
  digitalWrite(led_on,HIGH);  //On constant for now
  interruptSetup();  // sets up to read Pulse Sensor signal every 2mS 
  
  for(;;){  //Run continuously
  //  long loopTimer;
  //  loopTimer = millis();
  
    if ((millis() - sleepTimer) > 20000){  //Go to sleep after 20s of non-use
      interruptEnd();
      digitalWrite(led_on, LOW);  //Turn the pulse sensor off
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1);
      esp_light_sleep_start();
      ESP.restart();
    }
  

    if((millis()-edaReadTimer) >= 10){  //Read every ~10ms.
      int timediff = millis()-edaReadTimer;
      edaReadTimer = millis();
      int prev_eda = eda;
      eda = analogRead(EDA);
      char edaRead[10] = {0};
      sprintf(edaRead,"%d ", eda);
      strcat(edaReport, edaRead);
      char timeRead[10] = {0};
      sprintf(timeRead,"%d ", timediff);
      strcat(timestring, timeRead);
      if(eda > 25 && prev_eda > 25){  //If the device is being held, reset the sleep timer
        sleepTimer = millis();
      }
    }
    
    ICM.getAGMT();  // Update IMU Values
    accx = ICM.accX()*100;
    accy = ICM.accY()*100;
    accz = ICM.accZ()*100;
    mag_acc = sqrt((pow(accx, 2) + pow(accx, 2) + pow(accx, 2)));
    gyrx = ICM.gyrX()*100;
    gyry = ICM.gyrY()*100;
    gyrz = ICM.gyrZ()*100;
    mag_gyr = sqrt((pow(gyrx, 2) + pow(gyrx, 2) + pow(gyrx, 2)));
    brdtemp = ICM.temp()*100;

    skintemp = analogRead(ST);
    mic = analogRead(MIC);
    fsr = analogRead(FSR);

    int summaryArray[6] = {skintemp, mag_acc, mag_gyr, brdtemp, mic, fsr};
    baseReadCounter++;
    updateSummaryVals(summaryArray);

    if ((millis() - start_time) > 10000){
      if ((Pulse) && ((millis() - haptic_pulse_to) > 300)){
        haptic_pulse_to = millis();
        drv.setWaveform(0, 11);  // play effect 
        drv.setWaveform(1, 0);       // end waveform
        drv.go();  // play the effect!
      }
    }

    checkRollover();
    
    //Serial.print("Sesnor Loop Time: ");
    //Serial.println((millis() - loopTimer));
    yield();
  }
}

/*-----------------------------------------------------------------------------------------------*/

void updateSummaryVals(int summaryArray[10]){
  avg_st = avg_st + summaryArray[0];
  if(summaryArray[0] > max_st){
    max_st = summaryArray[0];
  }
  avg_acc = avg_acc + summaryArray[1];
  if(summaryArray[1] > max_acc){
    max_acc = summaryArray[1];
  }
  avg_gyr = avg_gyr + summaryArray[2];
  if(summaryArray[2] > max_gyr){
    max_gyr = summaryArray[2];
  }
  avg_brdtemp = avg_brdtemp + summaryArray[3];
  if(summaryArray[4] < min_mic){
    min_mic = summaryArray[4];
  }
  if(summaryArray[4] > max_mic){
    max_mic = summaryArray[4];
  }
  avg_fsr = avg_fsr + summaryArray[5];
  if(summaryArray[5] > max_fsr){
    max_fsr = summaryArray[5];
  }
}
