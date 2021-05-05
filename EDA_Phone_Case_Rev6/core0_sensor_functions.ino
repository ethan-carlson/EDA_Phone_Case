void core0Tasks(void * parameter){

  initializeIMU();
  
  drv.begin();
  drv.selectLibrary(1);

  // The interrupt setup especially has to stay here so the ISR runs on the right core
  digitalWrite(led_on,HIGH);  // Pulse LED on constant for now, could flash to save power
  pulseInterruptSetup();      // Sets up to read Pulse Sensor signal every 2mS
  EDAInterruptSetup();        // Sets the EDA read cycle to be 100ms
  
  for(;;){  //Run continuously
  //  long loopTimer;
  //  loopTimer = millis();

    /*  Go to sleep after 20s of non-use  */
    if ((millis() - sleepTimer) > 20000){
      pulseInterruptEnd();                 //Stop looking for heart beats
      EDAInterruptEnd();                   //Stop the EDA read ISR
      WiFi.disconnect();                   //Shut down WiFi gracefully
      digitalWrite(led_on, LOW);           //Turn the pulse sensor off
      esp_deep_sleep(30*60*1000*1000);     //Wake up every 30 minutes (in us) to report battery state
      ESP.restart();                       //This is redundant, as the deep sleep function does not return
    }
    
    ICM.getAGMT();  // Update IMU Values
    accx = ICM.accX()*100;
    accy = ICM.accY()*100;
    accz = ICM.accZ()*100;
    mag_acc = sqrt((pow(accx, 2) + pow(accy, 2) + pow(accz, 2)));
    gyrx = ICM.gyrX()*100;
    gyry = ICM.gyrY()*100;
    gyrz = ICM.gyrZ()*100;
    mag_gyr = sqrt((pow(gyrx, 2) + pow(gyry, 2) + pow(gyrz, 2)));
    brdtemp = ICM.temp()*100;

    if (mag_acc > 150000){      // If we sense a tap
      if (!firstTap){           // If it's our first tap, start the clock
        firstTap = true;        // Set the first tap flag
        tapTimer = millis();    // Start the lockout timer
      }
      else if ((millis() - tapTimer) > 200){  // If it's our second tap and the lockout has passed
        secondTap = true;       // Trigger a double tap
      }
    }
    if ((!secondTap) && ((millis() - tapTimer) > 1000)){    // If a second passes with no second tap
      firstTap = false;         // Reset the first tap flag
    }

    skintemp = analogRead(ST);
    mic = analogRead(MIC);
    fsr = analogRead(FSR);

    int summaryArray[6] = {skintemp, mag_acc, mag_gyr, brdtemp, mic, fsr};
    baseReadCounter++;
    updateSummaryVals(summaryArray);

    if (biofeedback){                 // If the biofeedback flag is enabled, playback heart beats on ERM
      if ((millis() - start_time) > 15000){
        if ((Pulse) && ((millis() - haptic_pulse_to) > 400)){
          haptic_pulse_to = millis();
          drv.setWaveform(0, 11);     // choose effect 
          drv.setWaveform(1, 0);      // end waveform
          drv.go();                   // play the effect!
        }
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
