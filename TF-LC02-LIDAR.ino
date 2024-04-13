/*
  Software serial to connect to the Evelta TF-LC02 LiDAR unit.

  The processing in this code is based on the TF-LC02 Product Manual: 
       https://evelta.com/content/datasheets/BP-UM-TF-LC02%20A01%20User%20Manual-EN.pdf
  It has been tested using a 3.3V TF-LC02 and an ESP32-WROOM-DA Module.

  Hal Breidenbach
  Ann Arbor, MI

  Co

 */


#define MSG_END 0xFA
#define MEASURE 0x81
#define CROSSTALK 0x82
#define OFFSET 0x83
#define RESET 0x84
#define GET_FACTORY 0x85
#define GET_PRODUCT 0x86

#define LINE_FEED 0x0A
#define CARRIAGE_RETURN 0x0D

#define RXD2 16
#define TXD2 17

int chipSelPin = 5;   // chip select

struct LiDAR_msg {
  char ID[2] = {0x55, 0xAA};
  char action;
  char length;
  char data[29];
  
};

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial2.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  digitalWrite(chipSelPin, LOW);  //  always selected, one device only

  Serial.println("Ready");
  print_menu();
  delay(200);    // time for TF-LC02 to wake up after power on
}

void loop() // run over and over
{
  if (Serial.available()){
    char in = Serial.read();
    if ((in != LINE_FEED) && (in != CARRIAGE_RETURN)){
      do_console(in);
    }
  }
}

/*
  Decode the command from the console.
*/
void do_console(char c){
  LiDAR_msg cmnd;
  bool result = true;
  Serial.printf("Request: %c \n\r", c);
  switch(c){
    case 'H':
    case 'h':
      print_menu();
      result = false; 
    break;
    case 'M':
    case 'm':
      cmnd.action = MEASURE;
      break;
    case 'C':
    case 'c':
      cmnd.action = CROSSTALK;
      break;
    case 'O':
    case 'o':
      cmnd.action = OFFSET;
    break;
    case 'R':
    case 'r':
      cmnd.action = RESET;
      break;
    case 'D':
    case 'd':
      cmnd.action = GET_FACTORY;
      break;
    case 'I':
    case 'i':
      cmnd.action = GET_PRODUCT;
      break;
    default:
      print_menu();
      result = false;
  }
  if (result){
    cmnd.length = 0;
    cmnd.data[0] = MSG_END;
    sendData(cmnd);
    delay(100);  // wait for processing
    receiveData(cmnd);
    parseResponse(cmnd);
  }
}

/*
  Send data to TF-LC02
*/
void sendData(LiDAR_msg & msg){
  Serial2.print(msg.ID[0]);
  Serial2.print(msg.ID[1]);
  Serial2.print(msg.action);
  Serial2.print(msg.length);
  for (int ndx = 0; ndx <= msg.length; ndx++){
    Serial2.print(msg.data[ndx]);
    }
  displayMsg(msg);
}

/*
  Display message hex values, uncomment body for diagnotic purposes
*/
void displayMsg(LiDAR_msg msg){
  /*
  Serial.printf("ID0 %x  ", msg.ID[0]);
  Serial.printf("ID1 %x  ", msg.ID[1]);
  Serial.printf("Action %x  ", msg.action);
  Serial.printf("Len %x  ", msg.length);
  for (int ndx = 0; ndx <= msg.length; ndx++){
    Serial.printf(" %x ", msg.data[ndx]);
  }  
  Serial.println(" ");
  */
}
/*
  Get the response from the TF-LC02
*/
void receiveData(LiDAR_msg & resp){
  resp.ID[0] = Serial2.read();
  resp.ID[1] = Serial2.read();
  resp.action = Serial2.read();
  resp.length = Serial2.read();
  if (resp.length < 30){
    for (int ndx = 0; ndx <= resp.length; ndx++){
      resp.data[ndx] = Serial2.read();
      }
  }
  displayMsg(resp);
}

/*
  Display the response from the TF-LC02
*/
void parseResponse(LiDAR_msg resp){
  switch (resp.action){
    case MEASURE:
      word dist;
      dist = ((resp.data[0] << 8) + resp.data[1]); 
      Serial.printf("Measure -- %d mm  ", dist);
      if (resp.data[2] > 0){
        printErrors(resp.data[2]);
      }
      else {
        Serial.println("  Valid result.");
      }
      
      break;
    case CROSSTALK:
      Serial.printf("Crosstalk: 0x%02x%02x \n\r", resp.data[2], resp.data[1]);
      if (resp.data[0] > 0){
        printErrors(resp.data[0]);
      }
      break;
    case OFFSET:
      Serial.printf("Offset correction -- Offset Short 1: 0x%02x, Offset Short 2: 0x%02x \n\r", resp.data[1], resp.data[2]);
      Serial.printf("                     Offset Long 1:  0x%02x, Offset Long 2:  0x%02x \n\r", resp.data[3], resp.data[4]);
      if (resp.data[0] > 0){
        printErrors(resp.data[0]);
      }
      break;
    case RESET:
      Serial.println("Module reset.");
      break;
    case GET_FACTORY:
      Serial.printf("Factory settings -- Offset Short 1: 0x%02x, Offset Short 2: 0x%02x \n\r", resp.data[0], resp.data[1]);
      Serial.printf("                    Offset Long 1:  0x%02x, Offset Long 2:  0x%02x \n\r", resp.data[2], resp.data[3]);
      Serial.printf("                    Crosstalk: 0x%02x%02x \n\r", resp.data[5], resp.data[4]);
      Serial.print("                    ");
      Serial.printf(getCalibrationResults(resp.data[6]));
      Serial.println(" ");
      break;
    case GET_PRODUCT:
      Serial.print("Product information -- product code: ");
      if (resp.data[0] == 0x02){
        Serial.print("gp2ap02vt");
      }
      else {
        Serial.print("gp2ap03vt");
      }
      Serial.print("  communication type: ");
      switch (resp.data[1]){
        case 'A':
          Serial.print("both UART and I2C  ");
        break;
        case 'I':
          Serial.print("I2C  ");
        break;
        case 'U':
          Serial.print("UART  ");
        break;
        default:
          Serial.print("error  ");
      }
      Serial.printf(" software level: %d", resp.data[2]);
      Serial.println(" ");
    break;
  }
}
/*
  Display the menu choices
*/
void print_menu() {
  Serial.println(" ");
  Serial.println("H for this menu");
  Serial.println("M for Measurement");
  Serial.println("C for Crosstalk correction and correction result");
  Serial.println("O for Offset correction and correction result");
  Serial.println("R for Reset the ToF module");
  Serial.println("D for Get factory default settings, including offset, X_TALK and check");
  Serial.println("I for Get product information, including model, communication mode and version");
}

/*
  Print error messages
  */

void printErrors(int errors){
  int err = 0x01;
  for (int ndx = 0; ndx < 8; ndx++){
    if (err & errors){
      Serial.print("  ");
      Serial.printf(getError(err));
      Serial.println(" ");
    }
    err <<= 1;
  }
}

/*
  Get measure error message
  */
char* getError(char err){
  switch (err){
    case 0x00:
      return("No errors.");
    case 0x01:
      return("VCSEL is short-circuited.");
    case 0x02:
      return("Amount of reflected light obtained from the detected object is small.\0");
    case 0x04:
      return("Ratio of reflected light from the detected object and disturbance light is small.");
    case 0x08:
      return("Disturbance light is large.\0");
    case 0x10:
      return("Wrapping error.\0");
    case 0x20:
      return("Internal calculation error.\0");
    case 0x80:
      return("Crosstalk from the panel is large.\0");
    default:
      return("Unknown error.  Module reset might help.");
  }
}

/*
  Get Calibration Results
*/

char* getCalibrationResults(char result){
  switch (result){
    case 1:
      return("Unchecked.");
    case 2:
      return("Crosstalk checked.");
    case 3:
      return("Offset checked.");
    case 4:
      return("Full checked.");
    default:
      return("Invalid value");
  }
}

