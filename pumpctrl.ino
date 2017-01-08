char junk;

#include <SPI.h> // #include <SPI.h>
#include <McpDigitalPot.h> // "lib/mcpdigitalpot/McpDigitalPot.h" //#include <McpDigitalPot.h>

// set the serial connection baud rate
const long SERIAL_BAUD = 38400; // 4800,9600,14400,19200,38400,57600,115200,0.5M,1.0M,2.0M

// this pin turns on and off the pump
const int CTRL_PIN = 3;
//const int SET_DELAY = 2000;
const int POT_PUMPSPEED = 1;
int pumpSpeed;
int loopDelay = 100;
bool cycleStopped = false;
const bool PUMP_ON = HIGH;
const bool PUMP_OFF = LOW;
bool pumpRunning = false;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long cycleInterval = 10000;
float dutyCycle = 50;

// string to hold serial inputs
String inputString = "";

// declaring stuff so it's global (bad constructors)
McpDigitalPot digitalPot1 = McpDigitalPot( 10, 10000 );
//McpDigitalPot digitalPot2 = McpDigitalPot( 9, 10000 );

// set up the MCP4261 digital potentiometers
void setup_pots() {
  /**
   * /////////////////////
   * /// SETUP MCP4261 ///
   * /////////////////////
   * Configured the Microchip digital potentiometers
   * Uses the Spi library by Cam Thompson, originally part of FPU library (micromegacorp.com)
   * from http://arduino.cc/playground/Code/Fpu or http://www.arduino.cc/playground/Code/Spi
   * Including SPI.h vv below initializea the MOSI, MISO, and SPI_CLK pins as per ATMEGA 328P
   */pinMode(CTRL_PIN, OUTPUT);

   // McpDigitalPot library available from https://github.com/teabot/McpDigitalPot
   // Adapted from https://github.com/dreamcat4/McpDigitalPot
   // Wire up the SPI Interface common lines:
   // #define SPI_CLOCK            13 //arduino <-> SPI Slave Clock Input   -> SCK (Pin 02 on McpDigitalPot DIP)
   // #define SPI_MOSI             11 //arduino <-> SPI Master Out Slave In -> SDI (Pin 03 on McpDigitalPot DIP)
   // #define SPI_MISO             12 //arduino <-> SPI Master In Slave Out -> SDO (Pin 13 on McpDigitalPot DIP)

   // Then choose any other free pin as the Slave Select (default pin 10)
   // each additional SPI device needs its own CS pin
   #define MCP_POT1_CS 10  //arduino   <->   Chip Select      -> CS  (Pin 01 on McpDigitalPot DIP)
   //#define MCP_POT2_CS 9   // for the second MCP4261

   // CALIBRATE ME: end-to-end potentiometer resistance, terminal A-terminal B
   float rAB_ohms = 10000.00; // 10k Ohm

   // CALIBRATE ME: rW = wiper resistance (75-160 ohms)
   //McpDigitalPot digitalPot = McpDigitalPot( MCP_DIGITAL_POT_SLAVE_SELECT_PIN, rAB_ohms, rW_ohms );
   // Instantiate McpDigitalPot object, with default rW (=117.5 ohm, its typical resistance)
   digitalPot1 = McpDigitalPot( MCP_POT1_CS, rAB_ohms );

   // Adding a second MCP4261...
   //digitalPot2 = McpDigitalPot( MCP_POT2_CS, rAB_ohms );

   // initialize SPI:
   SPI.begin();

   // (optional)
   // Set the scaling method, 100.0 for percentages, 1.0 for fractions
   digitalPot1.scale = 100.0;
   //digitalPot2.scale = 100.0;

   // set potentiometer initial positions
   //DEV_VOLT.setResistance( POT_VOLT, VOLT_INIT );
   //DEV_FREQ.setResistance( POT_FREQ, FREQ_INIT );
   //DEV_FLOW.setResistance( POT_FLOW, FLOW_INIT );
}

// set up the serial connection (over USB)
void setup_serial() {
  // initialize serial communication at specified baudrate
  Serial.begin(SERIAL_BAUD);
  pinMode(CTRL_PIN, OUTPUT);
}

// gets nputs recieved via serial connection
void get_serial() {
  //Serial.println("got serial");
  boolean stringComplete = false;  // whether the string is complete

  while (Serial.available() > 0) {
    // get the new byte:
    char inChar = (char)Serial.read();
    inputString += inChar;
    
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
      break;
    }
  }
  if ( stringComplete == true && inputString.length() > 0 ) {
    //Serial.println(inputString);
    manual_input(inputString);
    inputString = "";
  }
}

// process inputs recieved via serial connection
void manual_input(String input) {
  Serial.print(millis());
  Serial.print("; manual input: ");
  Serial.println(input);
  switch( input.charAt(0) ) {
    case 'p' :
      // you sent PAUSE
      //myRun = false;
      //stop_pump();
      pumpRunning = false;
      Serial.print(millis());
      Serial.println("; pump paused");
      break;

    case 'r' :
      // you sent RESUME
      //myRun = true;
      //start_pump();
      pumpRunning = true;
      Serial.print(millis());
      Serial.println("; pump resumed");
      break;

    case 's' :
      // you sent d,###
      //myInterval = input.substring(2).toInt();
      //Serial.println("delay set");
      pumpSpeed = input.substring(2).toInt();
      set_pumpspeed(pumpSpeed);
      break;

    case 'd' :
      // you sent d,###
      dutyCycle = input.substring(2).toFloat();
      Serial.print(millis());
      Serial.print("; duty cycle set: ");
      Serial.print(dutyCycle);
      Serial.println(" %");
      break;

    case 'c' :
      // you sent c,###
      cycleInterval = input.substring(2).toFloat();
      Serial.print(millis());
      Serial.print("; cycle interval set: ");
      Serial.print(cycleInterval);
      Serial.println(" milliseconds");
      break;
      }
  }

// set the pump speed with the digital potentiometer
void set_pumpspeed(int pumpSpeed) {
  digitalPot1.setResistance( POT_PUMPSPEED, pumpSpeed );
  Serial.print(millis());
  Serial.print("; pump speed set to ");
  Serial.print(pumpSpeed);
  Serial.println(" % of maximum");
  }

void start_pump() {
  digitalWrite(CTRL_PIN, PUMP_ON);
  Serial.print(millis());
  Serial.println("; pump on");
  }

void stop_pump() {
  digitalWrite(CTRL_PIN, PUMP_OFF);
  Serial.print(millis());
  Serial.println("; pump off");
  }

void setup() {
  setup_serial();
  setup_pots();
  set_pumpspeed(0);
  stop_pump();
  }

void loop() {
  currentMillis = millis();
  if (pumpRunning) {

    if (cycleStopped == false && 
        currentMillis - previousMillis >= cycleInterval*(dutyCycle/100)) {
      // turn off the pump
      stop_pump();
      cycleStopped = true;
      }

    if (currentMillis - previousMillis >= cycleInterval) {
      // keep track of the last time the on/off cycle completed
      previousMillis = currentMillis;
      cycleStopped = false;
      start_pump();
      }
  }

  else {
    previousMillis = currentMillis;
  }
  
  if (Serial.available() > 0) {
    get_serial();
    }

  delay(loopDelay);
}
