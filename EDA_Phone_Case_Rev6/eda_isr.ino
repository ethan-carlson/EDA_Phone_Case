
hw_timer_t * EDAtimer = NULL;

void EDAInterruptSetup(){     
  EDAtimer = timerBegin(1, 80, true);             // Use 2nd timer
  timerAttachInterrupt(EDAtimer, &ISReda, true);  // Attach ISReda function to our timer.
  timerAlarmWrite(EDAtimer, 100000, true);        // Set alarm to call isr function every 100 milliseconds
  timerAlarmEnable(EDAtimer);                     // Start the alarm
}

void EDAInterruptEnd(){     
  timerAlarmDisable(EDAtimer);
  timerDetachInterrupt(EDAtimer);
  timerEnd(EDAtimer);
} 

void ISReda(){                                // triggered when timer fires....
    int timediff = millis()-edaReadTimer;     // Verify the actual time between reads
    edaReadTimer = millis();
    int prev_eda = eda;                       // Set previous reading
    eda = analogRead(EDA);                    // Take new reading
    
    char edaRead[10] = {0};                   // Fill the appropriate report buffers
    sprintf(edaRead,"%d ", eda);
    strcat(edaReport, edaRead);
    char timeRead[10] = {0};
    sprintf(timeRead,"%d ", timediff);
    strcat(timestring, timeRead);
    
    if(eda > 5 && prev_eda > 5){  //If the device is being held, reset the sleep timer
      sleepTimer = millis();
    }
}// end isr
