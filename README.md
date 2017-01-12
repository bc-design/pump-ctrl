
# pump-ctrl
Arduino-based controller for a Watson-Marlow 323U peristaltic pump

### Arduino Scripts

+ **pumpctrl.ino** allows the cycle time, duty cycle, and pump speed to be set on the peristaltic pump. A serial monitor can be used to send commands to the controller and log the controller's actions:
  + `p`: pause the pump; the pump will keep doing what it's doing and will not cycle.
  + `r`: resume the pump; the pump will run normally.
  + `n,20`: set pump speed to 20% of maximum
  + `d,50`: set the duty cycle (on time vs. off time) to 50%
  + `c,60000`: set the cycle time (total time of one on-and-off cycle) to 60,000 milliseconds = 10 minutes

When the code is initially run, the system is paused: pumpSpeed = 0 and pumpRunning = false.  To get the pump to operate, in addition to setting a pumpSpeed with `s,###` you'll need to resume operation with `r`.

### Tools

+ **arsend** is a unix terminal script for sending commands to the Arduino via the serial connection
+ **arduino.rules** contains udev rules for automatically adding the Arduino as `/dev/arduino-uno`
