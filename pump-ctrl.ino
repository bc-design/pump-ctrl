char junk;
/*
  ## HARDWARE
  Watson-Marlow 323U peristaltic pump
  - interfaced to controller with 25-pin d-subminiature connector
  - three connections: GND, pump on/off, and pump speed
  Microchip MCP4261
  - 8-bit dual digital potentiometer
  - used as an analog output to control pump speed
*/

// the SPI library is needed to communicate with the digital potentiometer
#include <SPI.h>
// library for controlling the digital potentiometer
#include <McpDigitalPot.h> // "libraries/McpDigitalPot/McpDigitalPot.h" 

const String FIRMWARE_VERSION = "Running Arduino pumpctrl.ino, ver. 2017-01-19";
// set the serial connection baud rate
// valid rates: 4800,9600,14400,19200,38400,57600,115200,0.5M,1.0M,2.0M
const long SERIAL_BAUD = 38400; 
// this pin turns on and off the pump
const int CTRL_PIN = 3;
// specifies which pot (1,2) in the MCP4261 is tied to pump speed
const int POT_PUMPSPEED = 1;
// relationship between controller digital logic and pump state
const bool PUMP_ON = HIGH;
const bool PUMP_OFF = LOW;

// pump speed, percent
int pumpSpeed;
// pump duty cycle, percent
float dutyCycle = 50;
// total duration of an on/off cycle, ms
unsigned long msCycleInterval = 10000;
// clocktime of the current loop, ms
unsigned long msLoopCurrent = 0;
// clocktime of the previous loop, ms
unsigned long msLoopPrevious = 0;
// clocktime of the pump cycle start, ms
unsigned long msCycleStart = 0;

// string to hold incoming serial messages
String inputString = "";
// values of incoming pump setpoints
int setPumpSpeed;
int setPumpDuty;
float setPumpInterval;
// whether the pump is currently running
bool pumpRunning;
// whether the pump will run if controller is active
bool pumpRunningCtrl = true;
// whether the controller is active; false is paused
bool ctrlActive;
// delay between main loop iterations, milliseconds
const int loopDelay = 100;

// declaring stuff so it's global (bad constructors)
McpDigitalPot digitalPot1 = McpDigitalPot( 10, 10000 );
//McpDigitalPot digitalPot2 = McpDigitalPot( 9, 10000 );

// set up the MCP4261 digital potentiometers
void setup_pots() {
  // Uses the Spi library by Cam Thompson, originally part of FPU library (micromegacorp.com)
  // from http://arduino.cc/playground/Code/Fpu or http://www.arduino.cc/playground/Code/Spi
  // McpDigitalPot library available from https://github.com/teabot/McpDigitalPot
  // Adapted from https://github.com/dreamcat4/McpDigitalPot
  // Wire up the SPI Interface common lines:
  // #define SPI_CLOCK            13 //arduino <-> SPI Slave Clock Input   -> SCK (Pin 02 on McpDigitalPot DIP)
  // #define SPI_MOSI             11 //arduino <-> SPI Master Out Slave In -> SDI (Pin 03 on McpDigitalPot DIP)
  // #define SPI_MISO             12 //arduino <-> SPI Master In Slave Out -> SDO (Pin 13 on McpDigitalPot DIP)
  // Then choose any other free pin as the Slave Select (default pin 10)
  // each additional SPI device needs its own CS pin
  const int MCP_POT1_CS = 10;  //arduino   <->   Chip Select      -> CS  (Pin 01 on McpDigitalPot DIP)
  //const int MCP_POT2_CS = 9;   // for a second MCP4261, if used
  
  // CALIBRATE ME: end-to-end potentiometer resistance, terminal A-terminal B
  float rAB_ohms = 10000.00; // 10k Ohm
  
  // CALIBRATE ME: rW = wiper resistance (75-160 ohms)
  //McpDigitalPot digitalPot = McpDigitalPot( MCP_DIGITAL_POT_SLAVE_SELECT_PIN, rAB_ohms, rW_ohms );
  // Instantiate McpDigitalPot object, with default rW (=117.5 ohm, its typical resistance)
  digitalPot1 = McpDigitalPot( MCP_POT1_CS, rAB_ohms );
  // Adding a second MCP4261...
  //digitalPot2 = McpDigitalPot( MCP_POT2_CS, rAB_ohms );
  
  // initialize the SPI interface
  SPI.begin();
  
  // Set the scaling method, 100.0 for percentages, 1.0 for fractions
  digitalPot1.scale = 100.0;
  //digitalPot2.scale = 100.0;
  
  // set potentiometer initial positions
  //digitalPot1.setResistance( 1, POT1_INIT );
  //digitalPot1.setResistance( 1, POT1_INIT );
}

// set up the serial connection (over USB)
void setup_serial() {
  // initialize serial communication at specified baudrate
  Serial.begin(SERIAL_BAUD);
  // print the version of this program
  Serial.println();
  Serial.println(FIRMWARE_VERSION);
  Serial.println();
  // print a short README
  Serial.println("Controller starts with pumpSpeed = 0;");
  Serial.println("send e.g. `s,50` to set pumpSpeed to 50%");
  Serial.println("Controller starts paused with ctrlOn = false;");
  Serial.println("send `r` to resume the control loop");
  Serial.println("send `p` to pause the control loop");
  Serial.println("while the control loop is paused, manually control the pump:");
  Serial.println("send `m,start` to manually start the pump");
  Serial.println("send `m,stop` to manually stop the pump");
  Serial.println("when the control loop is resumed, it remembers its state.");
  Serial.println("set pump duty cycle with e.g. `d,50` for 50%");
  Serial.println("set total on+off cycle time with e.g. `c,60000` for 1 minute");
  Serial.println();
}

// gets inputs recieved via serial connection
void get_serial() {
  // keep track of whether a complete message has been received
  boolean msgComplete = false;
  // while there are characters in the serial input buffer...
  while (Serial.available() > 0) {
    // get the new byte
    char inChar = (char)Serial.read();
    // and add it to string
    inputString += inChar;
    // messages are terminated in a newline;
    // if the incoming character is a newline,
    // indicate that the message is complete
    if (inChar == '\n') {
      msgComplete = true;
      break;
    }
  }
  // if you've received a complete message of nonzero length
  if ( msgComplete && inputString.length() > 0 ) {
    // process the message
    process_msg(inputString);
    // delete the message
    inputString = "";
  }
}

// process messages recieved via serial connection
void process_msg(String input) {
  // log that a message has been received
  Serial.println();
  Serial.print(millis());
  Serial.print("; manual input: ");
  Serial.print(input);
  // parse the message and take the appropriate action
  switch( input.charAt(0) ) {
    case 'p' :
      // you sent PAUSE
      pause_ctrl();
      break;
      
    case 'r' :
      // you sent RESUME
      resume_ctrl();
      break;
      
    case 's' :
      // you sent s,###
      setPumpSpeed = input.substring(2).toInt();
      set_pumpspeed(setPumpSpeed);
      break;
      
    case 'd' :
      // you sent d,###
      setPumpDuty = input.substring(2).toInt();
      set_pumpduty(setPumpDuty);
      break;
      
    case 'c' :
      // you sent c,###
      setPumpInterval = input.substring(2).toInt();
      set_pumpcycleinterval(setPumpInterval);
      break;
      
    case 'm' :
      // you sent m,{start,stop}
      // MANUAL control of pump state
      if (input.substring(2) == "start") {
        // manually start the pump
        start_pump();
      }
      if (input.substring(2) == "stop") {
        // manually stop the pump
        stop_pump();
      }
      break;
  }
}

// pause the pump control loop
void pause_ctrl() {
  ctrlActive = false;
  // print a message to the serial log
  Serial.print(millis());
  Serial.println("; pump control loop paused");
  Serial.println();
}

// resume the pump control loop
void resume_ctrl() {
  ctrlActive = true;
  // print a message to the serial log
  Serial.print(millis());
  Serial.println("; pump control loop resumed");
  Serial.println();
  // if the pump was running when it was paused...
  if (pumpRunningCtrl) {
    // start the pump
    start_pump();
  }
  // if pump was NOT running when it was paused...
  else {
    // stop the pump
    stop_pump();
  }
  }
}

// set the pump speed with the digital potentiometer
void set_pumpspeed(int pumpSpeed) {
  digitalPot1.setResistance( POT_PUMPSPEED, pumpSpeed );
  // print a message to the serial log
  Serial.print(millis());
  Serial.print("; pump speed set to ");
  Serial.print(pumpSpeed);
  Serial.println(" % of maximum");
  Serial.println();
  }

// set the pump duty cycle
void set_pumpduty(int pumpDuty) {
  dutyCycle = pumpDuty;
  // print a message to the serial log
  Serial.print(millis());
  Serial.print("; duty cycle set: ");
  Serial.print(dutyCycle);
  Serial.println(" %");
  Serial.println();
  }

// set the pump on+off cycle duration
void set_pumpcycleinterval(unsigned long pumpCycleInterval) {
  msCycleInterval = pumpCycleInterval;
  // print a message to the serial log
  Serial.print(millis());
  Serial.print("; cycle interval set: ");
  Serial.print(msCycleInterval);
  Serial.println(" milliseconds");
  Serial.println();
  }

// turn the pump on
void start_pump() {
  digitalWrite(CTRL_PIN, PUMP_ON);
  pumpRunning = true;
  // print a message to the serial log
  Serial.print(millis());
  Serial.println("; pump on");
  }

// turn the pump off
void stop_pump() {
  digitalWrite(CTRL_PIN, PUMP_OFF);
  pumpRunning = false;
  // print a message to the serial log
  Serial.print(millis());
  Serial.println("; pump off");
  }

// the setup function runs once at controller startup
void setup() {
  // set up the serial connection
  setup_serial();
  // set up the pin that controls pump on/off state
  pinMode(CTRL_PIN, OUTPUT);
  // set up the digital potentiometer
  setup_pots();
  // set the pumpspeed to zero
  set_pumpspeed(0);
  // stop the pump
  stop_pump();
  // pause the control loop
  pause_ctrl();
  }

// loop function loops continuously after setup()
void loop() {
  // get the current clocktime
  msLoopCurrent = millis();

  // the controller implements duty cycle as follows:
  // if the controller is active:
  // - cycle starts and pump turns on
  // - when the pump has run for (cycle interval * duty cycle)...
  // - turn off the pump
  // - when the cycle interval has elapsed...
  // - turn on the pump
  // if the controller is paused:
  // - take no action
  // - increment the cycle start time
  if (ctrlActive) {
    // time since this pump control cycle started
    unsigned long msCycleElapsed = msLoopCurrent - msCycleStart;
    // if the pump is currently running and it's time to turn it off
    if (pumpRunning &&
        msCycleElapsed >= msCycleInterval*(dutyCycle/100.)) {
      // turn off the pump
      stop_pump();
      // mark the pump as off
      pumpRunningCtrl = false;
      }
    // if the end of the pump control cycle has been reached
    if (msCycleElapsed >= msCycleInterval) {
      // store the start time of the new cycle
      msCycleStart = msLoopCurrent;
      // turn on the pump
      start_pump();
      // mark the pump as on
      pumpRunningCtrl = true;
      }
  }
  
  // if the controller is NOT active
  else {
    // calculate time since last loop
    unsigned long msSinceLastLoop = msLoopCurrent - msLoopPrevious;
    // increment the cycle start time, as if time is stopped
    msCycleStart += msSinceLastLoop;
  }
  
  // if there is an incoming message
  if (Serial.available() > 0) {
    // get the message
    get_serial();
    }
  
  // store this loop's clocktime for later
  msLoopPrevious = msLoopCurrent;
  
  // do nothing for a few milliseconds before repeating the loop
  delay(loopDelay);
}
