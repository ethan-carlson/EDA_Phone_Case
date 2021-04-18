# Real Time UX
 
This is the repo for my Master's thesis project at MITidm, called 'Real Time UX', built on the HUZZAH32 development board from Adafruit and a custom PCB.

Real Time UX is a phone case that is capable of sensing when the person using the phone is stressed or frustrated, using biometric signals collected through the hand and fingers.  It uses the following sensors, not all of which may be useful for every application:
 - Electrodermal Activity (DC skin conductance)
 - Heart Rate and Heart Rate Variability (PPG)
 - Skin Temperature (Thermistor)
 - Device Motion (6-axis IMU)
 - Grasping Pressure (FSR)
 - Ambient Noise (Piezo Microphone) - Note that the max sampling rate of ~500 Hz is not fast enough to capture voice audio data

The device also utilizes an eccentric motor to generate haptic feedback, eg. for biofeedback applications.
