
void edaHTTP(){

  strcat(edaReport, timestring);  //combine the EDA and timestamp data to prep for the request buffer

  if(WiFi.status()== WL_CONNECTED){
    Serial.println("Sending EDA Request");
    HTTPClient http;
    http.begin(edaServer);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header
    String httpRequestData = String(edaReport);  // Data to send with HTTP POST
    int httpResponseCode = http.POST(httpRequestData);  // Send HTTP POST request
   
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
      
    http.end(); // Free resources
  }
  else {
    Serial.println("WiFi Disconnected");
  }        
}


void SummaryHTTP(){
  avg_st = avg_st / (baseReadCounter - 1);
  avg_accx = avg_accx / (baseReadCounter - 1);
  avg_accy = avg_accy / (baseReadCounter - 1);
  avg_accz = avg_accz / (baseReadCounter - 1);
  avg_gyrx = avg_gyrx / (baseReadCounter - 1);
  avg_gyry = avg_gyry / (baseReadCounter - 1);
  avg_gyrz = avg_gyrz / (baseReadCounter - 1);
  avg_brdtemp = avg_brdtemp / (baseReadCounter - 1);
  avg_mic = avg_mic / (baseReadCounter - 1);
  avg_fsr = avg_fsr / (baseReadCounter - 1);

  pulse = BPM;
  hrv = IBI;

  Serial.print("HR: ");
  Serial.println(BPM);
  Serial.print("IBI: ");
  Serial.println(IBI);

  float voltage = analogRead(BATT) * (6.6/4096);
  float batt_perc = ((voltage - 3.2) / 1.2) * 100.0;

  memset(summaryReport, 0, sizeof(summaryReport));
  sprintf(summaryReport, "AvgST=%d&AvgFSR=%d&AvgAccX=%d&AvgAccY=%d&AvgAccZ=%d&AvgGyrX=%d&AvgGyrY=%d&AvgGyrZ=%d&AvgBT=%d&"
                          "AvgMic=%d&MaxAccX=%d&MaxAccY=%d&MaxAccZ=%d&MaxGyrX=%d&MaxGyrY=%d&MaxGyrZ=%d&HR=%d&HRV=%.1f&"
                          "Batt=%.1f&MaxFSR=%d&MaxMic=%d",
                          avg_st, avg_fsr, avg_accx, avg_accy, avg_accz, avg_gyrx, avg_gyry, avg_gyrz, avg_brdtemp,
                          avg_mic, max_accx, max_accy, max_accz, max_gyrx, max_gyry, max_gyrz, pulse, hrv,
                          batt_perc, max_fsr, max_mic);

  max_st = 0;
  max_accx = 0;
  max_accy = 0;
  max_accz = 0;
  max_gyrx = 0;
  max_gyry = 0;
  max_gyrz = 0;
  max_fsr = 0;
  max_mic = 0;
  baseReadCounter = 0;

  if(WiFi.status()== WL_CONNECTED){
    Serial.println("Sending Summary Request");
    HTTPClient http;
    http.begin(summaryServer);

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header
    String httpRequestData = String(summaryReport);  // Data to send with HTTP POST
    int httpResponseCode = http.POST(httpRequestData);  // Send HTTP POST request
   
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
      
    http.end(); // Free resources
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}
