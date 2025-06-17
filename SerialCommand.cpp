#include "SerialCommand.h"

/**
 * Constructor makes sure some things are set.
 */
SerialCommand::SerialCommand(Stream &port,
                            const char* deviceType,
                            int writeEnablePin,
                            int maxCommands
                            )
  : _port(port),           // reference must be initialized right away
    _commandList(NULL),
    _commandCount(0),
    _term('\n'),           // default terminator for commands, newline character
    _last(NULL)
{
  _maxCommands = maxCommands;
  _device_type = deviceType; // Store the device type for command filtering
  _writeEnablePin = writeEnablePin;

  strcpy(_delim, " "); // strtok_r needs a null-terminated string
  clearBuffer();
  //allocate memory for the command list
  _commandList = (CommandInfo *) calloc(maxCommands, sizeof(CommandInfo));
  //NULL the default handler
  _default_command.function = NULL;
}

/**
 * Adds a "command" and a handler function to the list of available commands.
 * This is used for matching a found token in the buffer, and gives the pointer
 * to the handler function to deal with it.
 */
void SerialCommand::addCommand(const char *name, void (*function)(SerialCommand)) {
  #ifdef SERIALCOMMAND_DEBUG
    DEBUG_PORT.print("Adding command (");
    DEBUG_PORT.print(_commandCount);
    DEBUG_PORT.print("): ");
    DEBUG_PORT.println(name);
  #endif
  if (_commandCount >= _maxCommands){
      #ifdef SERIALCOMMAND_DEBUG
      DEBUG_PORT.print("Error: maxCommands was exceeded");
      #endif
      return;
    }
  //make a new callback
  struct CommandInfo new_command;
  new_command.name  = name;
  new_command.function = function;
  _commandList[_commandCount] = new_command;
  _commandCount++;
}

/**
 * This sets up a handler to be called in the event that the receveived command string
 * isn't in the list of commands.
 */
void SerialCommand::setDefaultHandler(void (*function)(SerialCommand)) {
  _default_command.function = function;
}

void SerialCommand::lookupCommandByName(char *name) {
  if (name != NULL) {
    bool matched = false;
    for (int i = 0; i < _commandCount; i++) {
      #ifdef SERIALCOMMAND_DEBUG
        DEBUG_PORT.print("Comparing [");
        DEBUG_PORT.print(name);
        DEBUG_PORT.print("] to [");
        DEBUG_PORT.print(_commandList[i].name);
        DEBUG_PORT.println("]");
      #endif

      // Compare the found command against the list of known commands for a match
      if (strcmp(name, _commandList[i].name) == 0) {
        #ifdef SERIALCOMMAND_DEBUG
        DEBUG_PORT.print("matched command: ");
        DEBUG_PORT.println(name);
        #endif
        _current_command = _commandList[i];
        
        matched = true;
        break;
      }
    }
    if (!matched) {
      #ifdef SERIALCOMMAND_DEBUG
      DEBUG_PORT.print("failed to match command with name: ");
      DEBUG_PORT.println(name);
      #endif
      _current_command = _default_command;
      _current_command.name = name;        //store the name
    }
  }
}


void SerialCommand::runCommand() {
    // Execute the stored handler function for the command,
    // passing in the "this" current SerialCommand object
    if (_current_command.function != NULL){
        (*_current_command.function)(*this);
    }
}



/**
 * This checks the Serial stream for characters, and assembles them into a buffer.
 * When the terminator character (default '\n') is seen, it starts parsing the
 * buffer for a prefix command, and calls handlers setup by addCommand() member
 */
int SerialCommand::readSerial() {
  #ifdef SERIALCOMMAND_DEBUG
  // DEBUG_PORT.println("in SerialCommand::readSerial()");
  #endif
  while (_port.available() > 0) {
    char inChar = _port.read();   // Read single available character, there may be more waiting
    //Serial.print(inChar);       // Echo back to serial stream
    #ifdef SERIALCOMMAND_DEBUG
    DEBUG_PORT.print(inChar);       // Echo back to serial stream
    #endif
    if (inChar == _term) {        // Check for the terminator (default '\r') meaning end of command
      #ifdef SERIALCOMMAND_DEBUG
        DEBUG_PORT.print("\tReceived: ");
        DEBUG_PORT.println(_buffer);
      #endif
      return _bufPos;
    }
    else if (isprint(inChar)) {     // Only printable characters into the buffer
      if (_bufPos < SERIALCOMMAND_BUFFER) {
        _buffer[_bufPos++] = inChar;  // Put character into buffer
        _buffer[_bufPos] = '\0';      // Null terminate
      } else {
        _bufPos = 0; //clear buffer;
        #ifdef SERIALCOMMAND_DEBUG
          DEBUG_PORT.println("\tLine buffer is full - increase SERIALCOMMAND_BUFFER");
        #endif
      }
    }
  }
  return 0;  //return zero until terminator encountered
}

void SerialCommand::matchCommand() {
  char *name = strtok_r(_buffer, _delim, &_last);   // Search for command_name at start of buffer
  lookupCommandByName(name);
}

/**
 * This checks the Serial stream for characters, and assembles them into a buffer.
 * When the terminator character (default '\n') is seen, it starts parsing the
 * buffer for a prefix command, and calls handlers setup by addCommand() member
 */
void SerialCommand::processCommand() {
  // matchCommand();
  char *name = strtok_r(_buffer, _delim, &_last);   // Search for command_name at start of buffer
  char *identifier = strtok_r(NULL, _delim, &_last);  // second token (identifier) for device type

  if (_device_type && strcmp(identifier, _device_type) == 0) {
    lookupCommandByName(name);
    runCommand();
  }
  clearBuffer();
}

/*
 * Set up the buffer with a command string
 */
void SerialCommand::setBuffer(char *text_line) {
  int  index = 0;
  char inChar = text_line[index];
  clearBuffer();
  while (inChar != '\0'){ //NULL terminated string
    if (inChar == _term) { // Check for the terminator (default '\r') meaning end of command
      return;
    }
    else if (isprint(inChar)) {     // Only printable characters into the buffer
      if (_bufPos < SERIALCOMMAND_BUFFER) {
        _buffer[_bufPos++] = inChar;  // Put character into buffer
        _buffer[_bufPos] = '\0';      // Null terminate
      } else {
        #ifdef SERIALCOMMAND_DEBUG
        DEBUG_PORT.println("Line buffer is full - increase SERIALCOMMAND_BUFFER");
        #endif
        return;
      }
    }
    index++;
    inChar = text_line[index];
  }
}

/*
 * Clear the input buffer.
 */
void SerialCommand::clearBuffer() {
  _buffer[0] = '\0';
  _bufPos = 0;
}

/**
 * Retrieve the next token ("word" or "argument") from the command buffer.
 * Returns NULL if no more tokens exist.
 */
char *SerialCommand::next() {
  return strtok_r(NULL, _delim, &_last);
}

/*
 * forward all writes to the encapsulated "port" Stream object
 */
size_t SerialCommand::write(uint8_t val) {
  return _port.write(val);
}

void SerialCommand::sendData(const char* message, char writeDelimiter) {
  /* send a message using the TX pin
    this message will be converted to 485, then goes to the FTDI chip.
    The writeEnablePin must be set high to allow the 485 chip to switch directions
  */
  digitalWrite(_writeEnablePin, HIGH);
  delayMicroseconds(500);
  _port.print(message);
  _port.print(writeDelimiter);
  _port.flush();
  delayMicroseconds(500);
  digitalWrite(_writeEnablePin, LOW);

  Serial.println(message);
}

