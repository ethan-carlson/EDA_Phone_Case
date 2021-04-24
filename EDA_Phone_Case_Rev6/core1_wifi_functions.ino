
void edaHTTP(){
  
  memset(edaRequest, 0, sizeof(edaRequest));  // Clear out the request buffer
  sprintf(edaRequest, edaReport);             // Fill the buffer with the EDA data
  strcat(edaRequest, timestring);             // Add the timestamp data
  
  memset(edaReport, 0, sizeof(edaReport));    // Clear out the other buffers so they can continue to be used
  sprintf(edaReport, "EDAstring=");
  memset(timestring, 0, sizeof(timestring));
  sprintf(timestring, "&timestring=");

  if(WiFi.status()== WL_CONNECTED){
    //Serial.println("Sending EDA Request");
    HTTPClient http;
    http.begin(edaServer);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header
    String httpRequestData = String(edaRequest);  // Data to send with HTTP POST
    int httpResponseCode = http.POST(httpRequestData);  // Send HTTP POST request
      
    http.end(); // Free resources
  }
  else {
    Serial.println("WiFi Disconnected");
  }        
}


void SummaryHTTP(){
  avg_st = avg_st / (baseReadCounter);
  avg_acc = avg_acc / (baseReadCounter);
  avg_gyr = avg_gyr / (baseReadCounter);
  avg_brdtemp = avg_brdtemp / (baseReadCounter);
  avg_fsr = avg_fsr / (baseReadCounter);

  int mic_rng = abs(max_mic - min_mic);
  
  float voltage = analogRead(BATT) * (6.6/4096);
  float batt_perc = ((voltage - 3.2)/0.8) * 100.0;

  memset(summaryReport, 0, sizeof(summaryReport));
  sprintf(summaryReport, "AvgST=%d&AvgFSR=%d&AvgAcc=%d&AvgGyr=%d&AvgBT=%d&"
                          "MaxAcc=%d&MaxGyr=%d&HR=%d&HRV=%.1f&"
                          "Batt=%.1f&MaxFSR=%d&MicRng=%d&Tap=%d",
                          avg_st, avg_fsr, avg_acc, avg_gyr, avg_brdtemp,
                          max_acc, max_gyr, BPM, hrv,
                          batt_perc, max_fsr, mic_rng, secondTap);

  //Serial.println(summaryReport);

  max_st = 0;
  max_acc = 0;
  max_gyr = 0;
  max_fsr = 0;
  max_mic = 0;
  min_mic = 4096;
  baseReadCounter = 1;
  if (secondTap){
    firstTap = false;
    secondTap = false;
  }

  if(WiFi.status()== WL_CONNECTED){
    //Serial.println("Sending Summary Request");
    HTTPClient http;
    http.begin(summaryServer);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header
    String httpRequestData = String(summaryReport);  // Data to send with HTTP POST
    int httpResponseCode = http.POST(httpRequestData);  // Send HTTP POST request
   
    //Serial.print("HTTP Response code: ");
    //Serial.println(httpResponseCode);
      
    http.end(); // Free resources
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}
