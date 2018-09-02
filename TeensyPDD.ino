/*
 * TeensyPDD - TPDD emulator fo Teensy 3.5 & 3.6 - Brian K. White bw.aljex.@gmail.com 20180824
 *  
 * Originally based on SD2TPDD v0.2 by Jimmy Pettit 07/27/2018
 *  
 *  Arduino IDE build options:
 *  * Tools -> Board: Teensy 3.6
 *  Production:
 *  * Tools -> USB -> No USB
 *  * Tools -> CPU Speed -> 2MHz
 *  * Tools -> Optimize: Fastest
 *  * "#undef CONS" below
 *  Devel/Debug:
 *  * Tools -> USB -> Serial
 *  * Tools -> CPU Speed -> 180MHz (any speed 24Mhz or more)
 *  * Tools -> Optimize: Fastest
 *  * "#define CONS" below
 */

// Serial port for tpdd protocol
#define CLIENT Serial1
#define CLIENT_BAUD 19200
// uncomment to enable RTS/CTS
#define CLIENT_RTS_PIN 21   //Teensy 3.5/3.6 can use any pin for RTS
#define CLIENT_CTS_PIN 20   //Teensy 3.5/3.6 can use pin 18 or 20 for CTS

// Debug console - where shall all console messages go
// Teensy can not use Serial (usb) below 24mhz
// Usually Serial or undefined, but could add oled etc for output-only display
// If undefined:
//   * All Serial (built-in usb) code is #ifdeffed out.
//   * Cpu can be underclocked all the way down to 2Mhz and it still works, at least well enough to satisfy TpddTool.py
//#define CONS Serial
#undef CONS

// "Drive Activity" light
#define DRIVE_ACTVITY
//#undef DRIVE_ACTVITY

#if defined(CONS)
 #define CPRINT(x)    CONS.print (x)
 #define CPRINTI(x,y)  CONS.print (x,y)
#else
 #define CPRINT(x)
 #define CPRINTI(x,y)
#endif

#if defined(DRIVE_ACTVITY)
// on-board LED - PORTB bit 5
#define PINMODE_LED_OUTPUT DDRB = DDRB |= 1UL << 5; // pinMode(13,OUTPUT);
#define LON PORTB |= _BV(5);                        // digitalWrite(13,HIGH);
#define LOFF PORTB &= ~_BV(5);                      // digitalWrite(13,LOW);
#else
#define PINMODE_LED_OUTPUT
#define LON
#define LOFF
#endif // DRIVE_ACTIVITY

#include <SdFat.h>
SdFatSdioEX SD;

File root;      // Root file for filesystem reference
File entry;     // Moving file entry for the emulator
File tempEntry; // Temporary entry for moving files

byte head = 0x00;               // Head index
byte tail = 0x00;               // Tail index

byte checksum = 0;              // Global variable for checksum calculation

byte state = 0;                 // Emulator command reading state
bool DME = false;               // TS-DOS DME mode flag

byte dataBuffer[256];           // Data buffer for commands
byte fileBuffer[0x80];          // Data buffer for file reading

char refFileName[25] = "";      // Reference file name for emulator
char refFileNameNoDir[25] = ""; // Reference file name for emulator with no ".<>" if directory
char tempRefFileName[25] = "";  // Second reference file name for renaming
char entryName[24] = "";        // Entry name for emulator
int directoryBlock = 0;         // Current directory block for directory listing
char directory[60] = "/";
byte directoryDepth = 0;
char tempDirectory[60] = "/";

void setup() {
  PINMODE_LED_OUTPUT

  CLIENT.begin(CLIENT_BAUD);
  CLIENT.clear();
  CLIENT.flush();
#if defined(CLIENT_RTS_PIN) && defined(CLIENT_CTS_PIN)
  CLIENT.attachRts(CLIENT_RTS_PIN);
  CLIENT.attachCts(CLIENT_CTS_PIN);
#endif 

#if defined(CONS) && CONS == Serial
  CONS.begin(115200);
  while(!CONS);
#endif

  CPRINT("TeensyPDD\r\n");

  clearBuffer(dataBuffer, 256); // Clear the data buffer

  while(initCard()>0) delay(1000);
}

int initCard () {

  //if (root.isOpen()) return 0;
  
  CPRINT("Opening SD card...");

  if (SD.begin()) {
    CPRINT("OK.\r\n");
  } else {
    CPRINT("No SD card.\r\n");
    return 1;
  }

  SD.chvol();
  root = SD.open(directory);  //Create the root filesystem entry

#if defined(CONS)
  printDirectory(root,0); //Print directory for debug purposes
#endif
  return 0;
}


/*
 * 
 * General misc. routines
 * 
 */

#if defined(CONS)
void printDirectory(File dir, int numTabs) { //Copied code from the file list example for debug purposes
  char fileName[24] = "";
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      CPRINT('\t');
    }
    entry.getName(fileName,24);
    CPRINT(fileName);
    if (entry.isDirectory()) {
      CPRINT("/\r\n");
      printDirectory(entry, numTabs + 1);
      // files have sizes, directories do not
      CPRINT("\t\t");
      CPRINTI(entry.fileSize(),DEC);
    } else {
      CPRINT("\r\n");
    }
    entry.close();
  }
}
#endif // CONS

void clearBuffer(byte* a, int l){ //Fills the buffer with 0x00
  for(int i=0; i<l; i++) a[i] = 0x00;
}
// TODO: less-stupid way than duplicating
void clearBufferC(char* a, int l){ //char version of clearBuffer()
  for(int i=0; i<l; i++) a[i] = 0x00;
}

// Copy a null-terminated char array to the directory array
void directoryAppend(char* c){
  bool terminated = false;
  int i = 0;
  int j = 0;
  
  while(directory[i] != 0x00) i++; // Jump i to first null character

  while(!terminated){
    directory[i++] = c[j++];
    terminated = c[j] == 0x00;
  }
  CPRINT(directory);
}

// Remove the top-most entry in the directoy path
void upDirectory(){
  int j = sizeof(directory);

  while(directory[j] == 0x00) j--; // Scan from end back to first(last) non-null
  if(directory[j] == '/' && j!= 0x00) directory[j] = 0x00; // Strip slash
  while(directory[j] != '/') directory[j--] = 0x00; // scan & clear until next slash
}

// copy working directory to scratchpad
void copyDirectory(){
  for(unsigned i=0; i<sizeof(directory); i++) tempDirectory[i] = directory[i];
}

/*
 * 
 * TPDD Port misc. routines
 * 
 */

void tpddWrite(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  CLIENT.write(c);
}

void tpddWriteString(char* c){  //Outputs a null-terminated char array c to the TPDD port
  int i = 0;
  while(c[i]!=0){
    checksum += c[i];
    CLIENT.write(c[i]);
    i++;
  }
}

void tpddSendChecksum(){  //Outputs the checksum to the TPDD port and clears the checksum
  CLIENT.write(checksum^0xFF);
  checksum = 0;
}


/*
 * 
 * TPDD Port return routines
 * 
 */
 
void return_normal(byte errorCode){ //Sends a normal return to the TPDD port with error code errorCode
  CPRINT("R:Norm ");
  CPRINTI(errorCode,HEX);
  CPRINT("\r\n");

  tpddWrite(0x12);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void return_reference(){  //Sends a reference return to the TPDD port
  byte term = 6;

  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  clearBufferC(tempRefFileName,24);  //Clear the reference file name buffer

  entry.getName(tempRefFileName,24);  //Save the current file entry's name to the reference file name buffer
  
  if(DME && entry.isDirectory()){ //      !!!Tacks ".<>" on the end of the return reference if we're in DME mode and the reference points to a directory
    for(int i=0; i < 7; i++){ //Find the end of the directory's name by looping through the name buffer
      if(tempRefFileName[i] == 0x00) term = i; //and setting a termination index to the offset where the termination is encountered
    }
    tempRefFileName[term++] = '.';  //Tack the expected ".<>" to the end of the name
    tempRefFileName[term++] = '<';
    tempRefFileName[term++] = '>';

    for(int i=term; i<24; i++) tempRefFileName[i] = 0x00; // fill rest of reference name with nulls
    term = 6; //Reset the termination index to prepare for the next check
  }

  for(int i=0; i<6; i++){ //      !!!Pads the name of the file out to 6 characters using space characters
    if(term == 6){  //Perform these checks if term hasn't changed
      if(tempRefFileName[i]=='.'){
        term = i;   //If we encounter a '.' character, set the temrination pointer to the current offset and output a space character instead
        tpddWrite(' ');
      }else{
        tpddWrite(tempRefFileName[i]);  //If we haven't encountered a period character, output the next character
      }
    }else{
      tpddWrite(' '); //If we did find a period character, write a space character to pad the reference name
    }
  }

  for(int i=0; i<18; i++) tpddWrite(tempRefFileName[i+term]); // write extension

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite((byte)((entry.fileSize()&0xFF00)>>8));  //File size most significant byte
  tpddWrite((byte)(entry.fileSize()&0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  CPRINT("R:Ref\r\n");
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port
  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  for(int i=0; i<24; i++){
    tpddWrite(0x00);  //Write the reference file name to the TPDD port
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite(0x00);  //File size most significant byte
  tpddWrite(0x00); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  CPRINT("R:BRef\r\n");
}

void return_parent_reference(){
  char r[] = "PARENT.<>";
  
  tpddWrite(0x11);
  tpddWrite(0x1C);
  
  tpddWriteString(r);
  for(int i=9; i<24; i++){  //Pad the rest of the data field with null characters
    tpddWrite(0x00);
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite(0x00);  //File size most significant byte
  tpddWrite(0x00); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
}

/*
 * 
 * TPDD Port command handler routines
 * 
 */

void command_reference(){ //Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+29)];  // The search form byte exists 29 bytes into the command
  byte refIndex = 0;  //Reference file name index
  
  CPRINT("SF:");
  CPRINTI(searchForm,HEX);
  CPRINT("\r\n");
  LON

  //if(!initCard()) return;
  
  if(searchForm == 0x00){ //Request entry by name
    for(int i=4; i<28; i++){  //Put the reference file name into a buffer
        if(dataBuffer[(byte)(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
          refFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //write it into the buffer and increment the index. 
        }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character

    CPRINT("Ref: ");
    CPRINT(refFileName);
    CPRINT("\r\n");

    if(DME){  //        !!!Strips the ".<>" off of the reference name if we're in DME mode
      if(strstr(refFileName, ".<>") != 0){
        for(int i=0; i<24; i++){  //Copies the reference file name to a scratchpad buffer with no directory extension if the reference is for a directory
          if(refFileName[i] != '.' && refFileName[i] != '<' && refFileName[i] != '>'){
            refFileNameNoDir[i]=refFileName[i];
          }else{
            refFileNameNoDir[i]=0x00; //If the character is part of a directory extension, don't copy it
          } 
        }
      }else{
        for(int i=0; i<24; i++){
          refFileNameNoDir[i]=refFileName[i]; //Copy the reference directly to the scratchpad buffer if it's not a directory reference
        }
      }
    }

    directoryAppend(refFileNameNoDir);  //Add the reference to the directory buffer

    if(SD.exists(directory)){ //If the file or directory exists on the SD card...
      entry=SD.open(directory); //...open it...
      return_reference(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry.
    }else{  //If the file does not exist...
      return_blank_reference();
    }

    upDirectory();  //Strip the reference off of the directory buffer
    
  }else if(searchForm == 0x01){ //Request first directory block
    root.close();
    root = SD.open(directory);
   ref_openFirst(); 
  }else if(searchForm == 0x02){ //Request next directory block
    root.close();
    root = SD.open(directory);
    ref_openNext();
  }else{  //Parameter is invalid
    return_normal(0x36);  //Send a normal return to the TPDD port with a parameter error
  }
  LOFF
}

void ref_openFirst(){
  directoryBlock = 0; //Set the current directory entry index to 0

  if(DME && directoryDepth>0 && directoryBlock==0){ //Return the "PARENT.<>" reference if we're in DME mode
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
}

void ref_openNext(){
  directoryBlock++; //Increment the directory entry index
  
  root.rewindDirectory(); //Pull back to the begining of the directory
  for(int i=0; i<directoryBlock-1; i++){  //skip to the current entry offset by the index
    root.openNextFile();
  }

  entry = root.openNextFile();  //Open the entry
  
  if(entry) {  //If the entry exists it is returned
    if(entry.isDirectory() && !DME){  //If it's a directory and we're not in DME mode
      entry.close();  //the entry is skipped over
      ref_openNext(); //and this function is called again
    }
    
    return_reference(); //Send the reference info to the TPDD port
    entry.close();  //Close the entry
  }else{
    return_blank_reference();
  }
}

void command_open(){  //Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+4)];  //The access mode is stored in the 5th byte of the command
  char s[] = "/";

  CPRINT("open mode:");
  CPRINT(rMode);
  CPRINT("\r\n");
  LON

  //if(!initCard()) return;

  entry.close();

  if(DME && strcmp(refFileNameNoDir, "PARENT") == 0){ //If DME mode is enabled and the reference is for the "PARENT" directory
    upDirectory();  //The top-most entry in the directory buffer is taken away
    directoryDepth--; //and the directory depth index is decremented
  } else {
    directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
    if(DME && (int)strstr(refFileName, ".<>") != 0 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      SD.mkdir(directory);  //create the directory
      upDirectory();
    } else {
      entry=SD.open(directory); //Open the directory to reference the entry
      if(entry.isDirectory()){  // !!!Moves into a sub-directory
        entry.close();          //If the entry is a directory
        directoryAppend(s);     //append a slash to the directory buffer
        directoryDepth++;       //and increment the directory depth index
      } else {                  //If the reference isn't a sub-directory, it's a file
        entry.close();
        switch(rMode){
          case 0x01: entry = SD.open(directory, FILE_WRITE); break; //Write
          case 0x02: entry = SD.open(directory, FILE_WRITE | O_APPEND); break;  //Append
          case 0x03: entry = SD.open(directory, FILE_READ); break;  //Read
        }
        upDirectory();
      }
    }
  }
  
  if(SD.exists(directory)){ //If the file actually exists...
    return_normal(0x00);  //...send a normal return with no error.
  }else{  //If the file doesn't exist...
    LOFF
    return_normal(0x10);  //...send a normal return with a "file does not exist" error.
  }
}

void command_close(){ //Closes the currently open entry
  CPRINT("close\r\n");
  //if(!initCard()) return;
  entry.close();  //Close the entry
  LOFF
  return_normal(0x00);  //Normal return with no error
}

void command_read(){  //Read a block of data from the currently open entry
  int bytesRead = entry.read(fileBuffer, 0x80); //Try to pull 128 bytes from the file into the buffer

  CPRINT("read A:");
  CPRINTI(entry.available(),HEX);
  CPRINT("\r\n");
  //if(!initCard()) return;

  if(bytesRead > 0){  //Send the read return if there is data to be read
    tpddWrite(0x10);  //Return type
    tpddWrite(bytesRead); //Data length
    for(int i=0; i<bytesRead; i++) tpddWrite(fileBuffer[i]);
    tpddSendChecksum();
  }else{
    return_normal(0x3F);  //send a normal return with an end-of-file error if there is no data left to read
  }
}

void command_write(){ //Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+3)];

  CPRINT("write\r\n");
  CPRINT(" length:");
  CPRINT(commandDataLength);
  CPRINT("\r\n");
  //if(!initCard()) return;

  for(int i=0; i<commandDataLength; i++){
    entry.write(dataBuffer[(byte)(tail+4+i)]);
  }
  
  return_normal(0x00);
}

void command_delete(){  //Delete the currently open entry
  CPRINT("delete\r\n");
  //if(!initCard()) return;

  entry.close();  //Close any open entries
  directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
  entry = SD.open(directory, FILE_READ);  //directory can be deleted if opened "READ"
  
  if(DME && entry.isDirectory()){
    entry.rmdir();  //If we're in DME mode and the entry is a directory, delete it
  }else{
    entry.close();  //Files can be deleted if opened "WRITE", so it needs to be re-opened
    entry = SD.open(directory, FILE_WRITE);
    entry.remove();
  }
  
  upDirectory();
  LOFF
  return_normal(0x00);  //Send a normal return with no error
}

void command_format(){  //Not implemented
  CPRINT("format\r\n");
  //if(!initCard()) return;
  return_normal(0x00);
}

void command_status(){  //Drive status
  CPRINT("status\r\n");
  //if(!initCard()) return;
  return_normal(0x00);
}

void command_condition(){ //Not implemented
  CPRINT("condition\r\n");
  //if(!initCard()) return;
  return_normal(0x00);
}

void command_rename(){  //Renames the currently open entry
  byte refIndex = 0;  //Temporary index for the reference name
  char s[] = "/";

  CPRINT("rename\r\n");
  //if(!initCard()) return;

  directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer
  
  if(entry){entry.close();} //Close any currently open entries
  entry = SD.open(directory); //Open the entry
  if(entry.isDirectory()){  //Append a slash to the end of the directory buffer if the reference is a sub-directory
    directoryAppend(s);
  }
  copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
  upDirectory();  //Strip the previous directory reference off of the directory buffer
  
  for(int i=4; i<28; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ //If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
  }
  
  tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character

  if(DME && entry.isDirectory()){ //      !!!If the entry is a directory, we need to strip the ".<>" off of the new directory name
    if(strstr(tempRefFileName, ".<>") != 0){
      // FIXME unsafe assumption deleting these chars this way?
      for(int i=0; i<24; i++){
        if(tempRefFileName[i] == '.' || tempRefFileName[i] == '<' || tempRefFileName[i] == '>'){
          tempRefFileName[i]=0x00;
        } 
      }
    }
  }

  directoryAppend(tempRefFileName);
  if(entry.isDirectory())directoryAppend(s);

  CPRINT(directory);
  CPRINT("\r\n");
  CPRINT(tempDirectory);
  CPRINT("\r\n");
  SD.rename(tempDirectory,directory);  //Rename the entry

  upDirectory();

  entry.close();
  
  return_normal(0x00);
}

/*
 * 
 * TS-DOS DME Commands
 * 
 */

void command_DMEReq(){  //Send the DME return with the root directory's name
  char r[] = " SDTPDD.<> ";
  CPRINT("DMEReq\r\n");
  //if(!initCard()) return;
  
  if(DME){
    tpddWrite(0x12);
    tpddWrite(0x0B);
    tpddWriteString(r); //Name must be 12 characters long and be space character padded
    tpddSendChecksum();
  }else{
    return_normal(0x36);
  }
}

void command_mystery1(){ //Not implemented
  CPRINT("mystery1\r\n");
  //if(!initCard()) return;
  return_normal(0x00);
}

void command_mystery2(){ //Not implemented
  CPRINT("mystery2\r\n");
  //if(!initCard()) return;
  return_normal(0x00);
}


/*
 * 
 * Main code loop
 * 
 */

void loop() {
  byte rType = 0;   // Current request type (command type)
  byte rLength = 0; // Current request length (command length)
  byte diff = 0;    // Difference between the head and tail buffer indexes

  state = 0; //0 = waiting for command 1 = waiting for full command 2 = have full command
  
  while(state<2){  // While waiting for a command...
    while (CLIENT.available() > 0){  // While there's data to read from the client...
      dataBuffer[head++] = (byte)CLIENT.read();  //...read a byte, increment the head index

      // if tail index == head index, a wrap-around has occurred! data will be lost!
      // increment tail index to prevent command size from overflowing
      if(tail==head) tail++;

      CPRINTI((byte)(head-1),HEX);
      CPRINT("-");
      CPRINTI(tail,HEX);
      CPRINTI((byte)(head-tail),HEX);
      CPRINT(":");
      CPRINTI(dataBuffer[head-1],HEX);
      CPRINT(";");
      CPRINT((dataBuffer[head-1]>=0x20)&&(dataBuffer[head-1]<=0x7E)?(char)dataBuffer[head-1]:' ');
      CPRINT("\r\n");
    }

    diff=(byte)(head-tail); //...set the difference between the head and tail index (number of bytes in the buffer)

    if(state == 0) { //...if we're waiting for a command...
      if(diff >= 4) { //...if there are 4 or more characters in the buffer...
        if(dataBuffer[tail]=='Z' && dataBuffer[(byte)(tail+1)]=='Z') { //...if the buffer's first two characters are 'Z' (a TPDD command)
          rLength = dataBuffer[(byte)(tail+3)]; //...get the command length...
          rType = dataBuffer[(byte)(tail+2)]; //...get the command type...
          state = 1; //...set the state to "waiting for full command".
        } else if(dataBuffer[tail]=='M' && dataBuffer[(byte)(tail+1)]=='1') { //If a DME command is received
          DME = true; //set the DME mode flag to true
          tail=tail+2; //and skip past the command to the DME request command
        } else { //...if the first two characters are not 'Z'...
          tail=tail+(tail==head?0:1); //...move the tail index forward to the next character, stop if we reach the head index to prevent an overflow.
        }
      }
    }

    if(state == 1) { //...if we're waiting for the full command to come in...
      if(diff>rLength+4){ //...if the amount of data in the buffer satisfies the command length...
        state = 2; //..set the state to "have full command".
      }
    }
  }

  CPRINTI(tail,HEX); //Debug code that displays the tail index in the buffer where the command was found...
  CPRINT("=");
  CPRINT("T:"); //...the command type...
  CPRINTI(rType, HEX);
  CPRINT("|L:"); //...and the command length.
  CPRINTI(rLength, HEX);
  CPRINT(DME?'D':'.');
  CPRINT("\r\n");

  switch(rType) {
    case 0x00: command_reference(); break;
    case 0x01: command_open(); break;
    case 0x02: command_close(); break;
    case 0x03: command_read(); break;
    case 0x04: command_write(); break;
    case 0x05: command_delete(); break;
    case 0x06: command_format(); break;
    case 0x07: command_status(); break;
    case 0x08: command_DMEReq(); break; // (drive condition)
    case 0x0C: command_condition(); break;
    case 0x0D: command_rename(); break;
    case 0x23: command_mystery2(); break;
    case 0x31: command_mystery1(); break;
    default: return_normal(0x36); CPRINT("ERR\r\n"); break;
  }

  CPRINTI(head,HEX);
  CPRINT(":");
  CPRINTI(tail,HEX);
  CPRINT("->");
  tail = tail+rLength+5;  //Increment the tail index past the previous command
  CPRINTI(tail,HEX);
  CPRINT("\r\n");

}
