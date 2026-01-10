Need to work on a major overhaul of our motor driver code

The firmware will support 3 motor drivers

The presence of one of these 3 steering setups will be detected in the following order:

- Keya Canbus
    - Detected by the presence of heartbeat 0x700001 messages on CANBUS 3.
    - Controlled by the current v26 code for Keya for speed, direction and kickout.
- Danfoss valve
    - Detected by PGN251 Byte 8 = 0x01
    - Controlled by Outputs 5 & 6. Output 5 HIGH to enable steering. Output 6:  Nominal 12v analog output. 50% = Center, 25% Full Left & 75% = Full right. 
    - Monitor kickout either from pressure sensor or wheel encoder, user selectable in AOG steering settings.
    - Danfoss + Wheel encoder is set by PGN251 Byte 8 = 0x01. 
    - Danfoss + Pressure sensor is set by PGN251 Byte 8 = 0x03
- DRV8701P
    - This will be default if Keya or Danfoss are not detected.
    - PWM1 will move the motor LEFT. PWM 2 will move the motor RIGHT.
    - Whichever PWM is not activated to move the motor will be se to LOW (0). 
    - Kickout will be monitored by either wheel encoder, pressure sensor or DRV8701P current output.
    - Wheel Encoder is set by PGN251 Byte 8  = 0x00.
    - Pressure Sensor is seen by PGN251 Byte 8 = 0x02.
    - Current Sensor is set by PGN251 Byte 8 = 0x04.

Since this is major work, please create a phased work plan that includes detection logic, motor driving logic and kickout logic implementation phases.. The plan should have stopping points to test, debug and commit each phase. 

These files have the primary current implementation for much of this work.
/Users/chris/Documents/Code/Firmware_Teensy_AiO-NG-v6/include/AutosteerPID.h
/Users/chris/Documents/Code/Firmware_Teensy_AiO-NG-v6/include/Autosteer.h
/Users/chris/Documents/Code/Firmware_Teensy_AiO-NG-v6/include/common.h
/Users/chris/Documents/Code/Firmware_Teensy_AiO-NG-v6/include/pcb.h

Other files in /Users/chris/Documents/Code/Firmware_Teensy_AiO-NG-v6/include/ may also have some details about the current implementation.