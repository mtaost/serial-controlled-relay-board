/*
   Serial Relay Controller
   Author: Michael Tao
   Date: 8/9/2018

   This is a modified version that uses the hardware serial to communicate over usb for demonstration purposes. 

   Credit to https://github.com/kroimon/Arduino-SerialCommand for inspiration for serial command line interpreter
*/

#define NUM_COMMANDS 5
#define BUFFER_SIZE 64
#define MAX_COMMAND_SIZE 16
#define MAX_JUMPER_NAME_SIZE 16
#define NUM_JUMPERS 8
#define BAUD_RATE 115200
#define PROMPT "# "
#define DELIMINATOR " "

// Pin definitions
// These are specific to how the board is routed so they're here for reference
#define J1_PIN 9
#define J2_PIN 8
#define J3_PIN 7
#define J4_PIN 6
#define J5_PIN 5
#define J6_PIN 4
#define J7_PIN 3
#define J8_PIN 2
#define TX 10
#define RX 11
#define STATUS_LED 12

//SoftwareSerial Serial = SoftwareSerial(RX, TX);
// Buffer for storing input so far
char buffer[BUFFER_SIZE];
int buffPos = 0;

// Counter to newest position in command list
int numCommands = 0;

// Struct to keep track of command names and their function call
struct Command {
  char name[MAX_COMMAND_SIZE];
  void (*function)();
};
Command commandList[NUM_COMMANDS];

// Struct to keep track of jumper names and their state
struct Jumper {
  char name[MAX_JUMPER_NAME_SIZE];
  int pin;
  boolean state = false; // true for on, false for off
};
Jumper jumperList[NUM_JUMPERS];

// Flag to keep track of whether we've sent the prompt or not
boolean prompt_sent;

// Terminal character (generally set to \r)
char term;

// Something for strtok_r to remember what it last tokenized. See strtok_r documentation for details
char* last;


void setup() {
  pinMode(STATUS_LED, OUTPUT);
  pinMode(J1_PIN, OUTPUT);
  pinMode(J2_PIN, OUTPUT);
  pinMode(J3_PIN, OUTPUT);
  pinMode(J4_PIN, OUTPUT);
  pinMode(J5_PIN, OUTPUT);
  pinMode(J6_PIN, OUTPUT);
  pinMode(J7_PIN, OUTPUT);
  pinMode(J8_PIN, OUTPUT);

  Serial.begin(BAUD_RATE);

  // Setup callbacks for commands
  addCommand("help", help);
  addCommand("set", set);
  addCommand("status", status);
  addCommand("rename", rename);
  addCommand("reset", reset);

  Serial.println('\f');
  Serial.println("Serial Relay Controller ready");
  blink(3);

  // Set jumper values to their defaults
  defaultJumperInit();

  // Establish the buffer and other strings reader needs to function
  initReader();
}

// In the loop, we send the prompt if it hasn't been sent, and then just call readSerial() to wait for user input
void loop() {
  if (!prompt_sent) {
    Serial.print(PROMPT);
    prompt_sent = true;
    blink(1);
  }
  readSerial();
}


// Just prints user manual
void help() {
  Serial.println("Available commands: ");
  Serial.println("   set jumper_name|all {on|off}");
  Serial.println("       Sets jumper with name 'jumper_name' on or off. If jumper name is 'all', will set all jumpers.\n");
  Serial.println("   status [jumper_name]");
  Serial.println("       Shows whether jumper is on or off. If no jumper or 'all' is provided, will display all jumper statuses\n");
  Serial.println("   rename old_name new_name");
  Serial.println("       Renames a jumper. Cannot use an existing name or 'all'\n");
  Serial.println("   reset");
  Serial.println("       Resets jumper names back to defaults. Sets all jumpers to off.\n");
  Serial.println("   help");
  Serial.println("       Displays this help dialogue.");
}


// Blink method for visual confirmation that things are working. Not really used though
void blink(int i) {
  while (i > 0) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
    i--;
  }
}

// Sets a jumper to on or off. This is called after the "set" part of the command has been consumed, so jumperName and state are obtained
// by calling nextToken() which will consume the next two tokens (if there are any)
void set() {
  char *jumperName = nextToken();
  char *state = nextToken();
  int jumper_index;
  boolean setTo;
  boolean lastState;

  if (jumperName == NULL) {
    Serial.println("Missing jumper");
    Serial.println("Syntax: set jumper {on/off}");
    Serial.println("        set all    {on/off}");
    return;
  }

  if (state == NULL) {
    Serial.println("Please specify a value for state");
    Serial.println("Syntax: set jumper {on/off}");
    Serial.println("        set all    {on/off}");
    return;
  }

  if (strcmp(state, "on") == 0) {
    setTo = true;
  }
  else if (strcmp(state, "off") == 0) {
    setTo = false;
  }
  // If we don't match on or off, invalid state
  else {
    Serial.println("Please specify either 'on' or 'off'");
    return;
  }

  // Get the index of the jumper we're looking for
  jumper_index = findJumper(jumperName);

  // If we don't find the jumper name
  if (jumper_index == -1) {
    // If jumper name is all (case insensitive), loop through all jumpers and set them
    if (strcmp(strlwr(jumperName), "all") == 0) {
      for (int i = 0; i < NUM_JUMPERS; i++) {
        Jumper j = jumperList[i];
        lastState = j.state;
        j.state = setTo;
        jumperList[i] = j;

        Serial.print(j.name);
        Serial.print("\t: ");
        lastState ? Serial.print("ON ") : Serial.print("OFF");
        Serial.print(" -> ");
        setTo ? Serial.println("ON") : Serial.println("OFF");

        updateState(i);
      }
    }

    // Otherwise for jumper_index == -1, jumper is invalid
    else {
      Serial.print(jumperName);
      Serial.println(" is not a valid jumper");
      Serial.println("Use 'status' for a list of valid jumpers");
    }
  }

  // If we have a jumper index, set the jumper
  else {
    Jumper j = jumperList[jumper_index];
    lastState = j.state;
    j.state = setTo;
    jumperList[jumper_index] = j;

    Serial.print(jumperName);
    Serial.print(": ");
    lastState ? Serial.print("ON") : Serial.print("OFF");
    Serial.print(" -> ");
    setTo ? Serial.println("ON") : Serial.println("OFF");

    updateState(jumper_index);
  }
}

// Renames a jumper, cannot name a jumper any variation of 'all' to avoid confusion with the keyword used in 'set all on/off'
void rename() {
  char *oldName = nextToken();
  char *newName = nextToken();
  int jumperIndex;

  if (oldName == NULL) {
    Serial.println("Missing jumper_name");
    Serial.println("Syntax: rename jumper_name new_name");
    return;
  }
  if (newName == NULL) {
    Serial.println("Missing new_name");
    Serial.println("Syntax: rename jumper_name new_name");
    return;
  }

  // Do not allow a jumper to be named "ALL"
  if (strcmp(newName, "all") == 0) {
    Serial.println("Jumper cannot be named 'all'");
    return;
  }

  jumperIndex = findJumper(oldName);

  // Invalid jumper option
  if (jumperIndex == -1) {
    Serial.print(oldName);
    Serial.println(" is not a valid jumper");
  }

  // New jumper name already exists
  else if (findJumper(newName) != -1) {
    Serial.print(newName);
    Serial.println(" already exists! Please pick a different name.");
  }

  // Otherwise, rename jumper
  else {
    strcpy(jumperList[jumperIndex].name, newName);
    Serial.print(oldName);
    Serial.print(" renamed to: ");
    Serial.println(newName);
  }
}

// Call init again and reset jumpers to their starting values
void reset() {
  defaultJumperInit();
  Serial.println("Jumper names reset to default values");
  Serial.println("All jumpers now off");
}

// Shows the status of a jumper, or all jumpers
void status() {
  char *jumperName = nextToken();
  int jumperIndex;
  boolean state;

  // If no argument is provided, or the argument is all, list all jumpers
  if (jumperName == NULL || strcmp(jumperName, "all") == 0) {
    char *name;
    for (int i = 0; i < NUM_JUMPERS; i++) {
      state = jumperList[i].state;
      name = jumperList[i].name;
      Serial.print(name);
      Serial.print("\t:");
      state ? Serial.println("ON") : Serial.println("OFF");
    }
    return;
  }

  jumperIndex = findJumper(jumperName);

  // Invalid jumper option
  if (jumperIndex == -1) {
    Serial.print(jumperName);
    Serial.println(" is not a valid jumper");
  }

  else {
    state = jumperList[jumperIndex].state;
    Serial.print(jumperName);
    Serial.print("\t:");
    state ? Serial.println("ON") : Serial.println("OFF");
  }
}

// Applies state of jumper to respective digital pin
// Assumes jumperIndex is valid
void updateState(int jumperIndex) {
  boolean state;
  String str;
  int pin;

  state = jumperList[jumperIndex].state;
  pin = jumperList[jumperIndex].pin;
  
//  Serial.print("Wrote ");
//  str = state ? "ON" : "OFF";
//  Serial.print(str);
//  Serial.print(" to pin ");
//  Serial.println(pin);
  
  digitalWrite(pin, state);
}

// Input name of jumper, returns index of jumper in jumperList
// Returns -1 if not found
int findJumper(char *jumperName) {
  int curr_jumper;
  boolean found_jumper = false;
  for (curr_jumper = 0; curr_jumper < NUM_JUMPERS; curr_jumper++) {
    if (strcmp(jumperName, jumperList[curr_jumper].name) == 0) {
      found_jumper = true;
      break;
    }
  }
  return found_jumper ? curr_jumper : -1;
}

void addCommand(const char *command, void (*function)()) {
  strncpy(commandList[numCommands].name, command, MAX_COMMAND_SIZE);
  commandList[numCommands].function = function;
  numCommands++;
}

void readSerial() {
  while (Serial.available() > 0) {
    char nextChar;
    char* token;
    boolean matched = false;
    nextChar = Serial.read();

    // If the next character matches the term (probably '\r'), process command
    if (nextChar == term) {
      prompt_sent = false;
      Serial.println();
      buffPos = 0;
      // Tokenize what we have so far and get the first token separated by deliminator (probably " ")
      token = strtok_r(buffer, DELIMINATOR, &last);
      // Empty string input, ignore
      if (token == NULL) {
        return;
      }
      else {
        for (int i = 0; i < NUM_COMMANDS; i++) {
          if (strcmp(token, commandList[i].name) == 0) {
            (*commandList[i].function)();
            Serial.println();
            clearBuffer();
            matched = true;
            break;
          }
        }
      }
      if (matched == false) {
        Serial.print('\r');
        Serial.print(buffer);
        Serial.println(" is not a valid command");
        Serial.println("Enter 'help' for available commands\n");
        clearBuffer();
      }
    }

    // When user presses backspace, delete character off buffer and reflect change in terminal
    if (nextChar == '\b') {
      if (buffPos > 0) {
        buffer[--buffPos] = '\0';
        Serial.print("\b \b");
      }
    }

    // If the input character is printable, add it to the buffer and send character to terminal
    if (isprint(nextChar)) {
      if (buffPos < BUFFER_SIZE - 1) { // Prevent overflow and clobbering memory
        buffer[buffPos++] = nextChar;
        buffer[buffPos] = '\0';
        Serial.print(nextChar);
      }
    }
  }
}

// Initialize some starting values
void initReader() {
  prompt_sent = false;
  term = '\r';
  memset(buffer, 0, BUFFER_SIZE);
}

// Reset the buffer, easiest thing to do is to set first char in array to null terminator \0
void clearBuffer() {
  buffPos = 0;
  buffer[buffPos] = '\0';
}

// Consumes the next token in the command. This works since strtok_r remembers its last tokenized string if it is passed in NULL
char *nextToken() {
  char *nextToken;
  nextToken = strtok_r(NULL, " ", &last);
  return nextToken;
}

// Initialize jumper names to default values. A little hard coded 
void defaultJumperInit() {
  char *names[8] = {"J1", "J2", "J3", "J4", "J5", "J6", "J7", "J8"};
  int pins[8] = {J1_PIN, J2_PIN, J3_PIN, J4_PIN, J5_PIN, J6_PIN, J7_PIN, J8_PIN};
  for (int i = 0; i < NUM_JUMPERS; i++) {
    strcpy(jumperList[i].name, names[i]);
    Jumper j = jumperList[i];
    j.state = false;
    j.pin = pins[i];
    jumperList[i] = j;
    updateState(i);
  }
}

