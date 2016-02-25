#include "Arduino.h"

#include <SPI.h>

// Arghhh. These three links have disappeared!
// See this page for the original code:
// http://www.raspberryginger.com/jbailey/minix/html/dir_acf1a49c3b8ff2cb9205e4a19757c0d6.html
// From: http://www.raspberryginger.com/jbailey/minix/html/zm_8c-source.html
// docs at: http://www.raspberryginger.com/jbailey/minix/html/zm_8c.html
// The minix files here might be the same thing:
// http://www.cise.ufl.edu/~cop4600/cgi-bin/lxr/http/source.cgi/commands/zmodem/

#include "zmodem_config.h"

#include "zmodem.h"
#include "zmodem_zm.h"

// This works with Tera Term Pro Web Version 3.1.3 (2002/10/08)
// (www.ayera.com) but TeraTerm only works on COM1, 2, 3 or 4.

// It DOES NOT handle interruptions of the Tx or Rx lines so it
// will NOT work in a hostile environment.

char *Progname = "Arduino ZModem V2.0";
  
/*
  Originally was an example by fat16lib of reading a directory
  and listing its files by directory entry number.
See: http://forum.arduino.cc/index.php?topic=173562.0

  Heavily modified by Pete (El Supremo) to recursively list the files
  starting at a specified point in the directory structure and then
  use zmodem to transmit them to the PC via the ZSERIAL port

  Further heavy modifications by Dylan (monte_carlo_ecm, bitflipper, etc.)
  to create a user driven "file manager" of sorts.
  Many thanks to Pete (El Supremo) who got this started.  Much work remained
  to get receive (rz) working, mostly due to the need for speed because of the
  very small (64 bytes) Serial buffer in the Arduino.

  I have tested this with an Arduino Mega 2560 R3 interfacing with Windows 10
  using Hyperterminal, Syncterm and TeraTerm.  All of them seem to work, though
  their crash recovery (partial file transfer restart) behaviours vary.
  Syncterm kicks out a couple of non-fatal errors at the beginning of sending
  a file to the Arduino, but appears to always recover and complete the transfer.

  I expect this will only work on Arduino Mega and perhaps Due and Teensy 3.1.
  The sketch uses over 40K of program storage, and ~4K of dynamic memory.  I 
  did what I could to move the CRC tables into PROGMEM, but I'm currently 
  skeptical that enough could be done to trim this down to (say) under 2K of 
  dynamic memory usage for the Uno.  

V2.0
2015-02-23
  - Taken over by Dylan (monte_carlo_ecm, bitflipper, etc.)
  - Added Serial based user interface
  - Added support for SparkFun MP3 shield based SDCard (see zmodem_config.h)
  - Moved CRC tables to PROGMEM to lighten footprint on dynamic memory (zmodem_crc16.cpp)
  - Added ZRQINIT at start of sz.  All terminal applications I tested didn't strictly need it, but it's
    super handy for getting the terminal application to auto start the download
  - Completed adaptation of rz to Arduino
  - Removed directory recursion for sz in favour of single file or entire current directory ("*") for sz
  - Optimized zdlread, readline, zsendline and sendline
      into macros for rz speed - still only up to 57600 baud
  - Enabled "crash recovery" for both sz and rz.  Various terminal applications may respond differently
      to restarting partially completed transfers; experiment with yours to see how it behaves.  This
      feature could be particularly useful if you have an ever growing log file and you just need to
      download the entries since your last download from your Arduino to your computer.

V1.03
140913
  - remove extraneous code such as the entire main() function
    in sz and rz and anything dependent on the vax, etc.
  - moved purgeline, sendline, readline and bttyout from rz to zm
    so that the the zmodem_rz.cpp file is not required when compiling
    sz 
    
V1.02
140912
  - yup, sz transfer still works.
    10 files -- 2853852 bytes
    Time = 265 secs
    
V1.01
140912
This was originally working on a T++2 and now works on T3
  - This works on a T3 using the RTC/GPS/uSD breadboard
    It sent multiple files - see info.h
  - both rz and sz sources compile together here but have not
    yet ensured that transmit still works.
    
V1.00
130630
  - it compiles. It even times out. But it doesn't send anything
    to the PC - the TTYUSB LEDs don't blink at all
  - ARGHH. It does help to open the Serial1 port!!
  - but now it sends something to TTerm but TTerm must be answering
    with a NAK because they just repeat the same thing over
    and over again.

V2.00
130702
  - IT SENT A FILE!!!!
    It should have sent two, but I'll take it!
  - tried sending 2012/09 at 115200 - it sent the first file (138kB!)
    but hangs when it starts on the second one. The file is created
    but is zero length.
    
  - THIS VERSION SENDS MULTIPLE FILES

*/

#include <SdFat.h>
#include <SdFatUtil.h>

#ifndef SFMP3_SHIELD
#define SD_SEL 9
#endif

SdFat sd;

#ifdef SFMP3_SHIELD
#include <SFEMP3Shield.h>
#include <SFEMP3ShieldConfig.h>
#include <SFEMP3Shieldmainpage.h>

SFEMP3Shield mp3;
#endif

#define error(s) sd.errorHalt(s)
const size_t MAX_FILE_COUNT = 50;
#define MAXPATH 64

extern int Filesleft;
extern long Totalleft;

extern SdFile fout;

void help(void)
{
  DSERIAL.print(Progname);
  DSERIAL.print(F(" - Transfer rate: "));
  DSERIAL.println(ZMODEM_SPEED);
  DSERIAL.println(F("Available Commands:"));
  DSERIAL.println(F("HELP     - Print this list of commands"));
  DSERIAL.println(F("DIR      - List files in current working directory - alternate LS"));
  DSERIAL.println(F("PWD      - Print current working directory"));
  DSERIAL.println(F("CD       - Change current working directory"));
  DSERIAL.println(F("DEL file - Delete file - alternate RM"));
  DSERIAL.println(F("MD  dir  - Create dir - alternate MKDIR"));
  DSERIAL.println(F("RD  dir  - Delete dir - alternate RMDIR"));
  DSERIAL.println(F("SZ  file - Send file from Arduino to terminal (* = all files)"));
  DSERIAL.println(F("RZ       - Receive a file from terminal to Arduino (Hyperterminal sends this"));
  DSERIAL.println(F("              automatically when you select Transfer->Send File...)"));
}

void setup()
{
  unsigned long total_bytes;
  int total_files;

// NOTE: The following line needs to be uncommented if DSERIAL and ZSERIAL are decoupled again for debugging
//  DSERIAL.begin(115200);

  ZSERIAL.begin(ZMODEM_SPEED);
  ZSERIAL.setTimeout(TYPICAL_SERIAL_TIMEOUT);

//  DSERIAL.println(Progname);
  
//  DSERIAL.print(F("Transfer rate: "));
//  DSERIAL.println(ZMODEM_SPEED);

  //Initialize the SdCard.
//DSERIAL.println(F("About to initialize SdCard"));
  if(!sd.begin(SD_SEL, SPI_FULL_SPEED)) sd.initErrorHalt(&DSERIAL);
  // depending upon your SdCard environment, SPI_HALF_SPEED may work better.
//DSERIAL.println(F("About to change directory"));
  if(!sd.chdir("/", true)) sd.errorHalt(F("sd.chdir"));
//DSERIAL.println(F("SdCard setup complete"));

  #ifdef SFMP3_SHIELD
  mp3.begin();
  #endif

  sd.vwd()->rewind();

  help();
 
}

int count_files(int *file_count, long *byte_count)
{
  uint16_t idx;
  SdFile file;
  dir_t dir;
  char name[13];
   
  *file_count = 0;
  *byte_count = 0;
  
  sd.vwd()->rewind();

  while (sd.vwd()->readDir(&dir) == sizeof(dir)) {
    // read next directory entry in current working directory

    // format file name
    SdFile::dirName(&dir, name);

    // remember position in directory
    uint32_t pos = sd.vwd()->curPosition();
     
    // open file
    if (!file.open(name, O_READ)) error(F("file.open failed"));
    
    // restore root position
    else if (!sd.vwd()->seekSet(pos)) error(F("seekSet failed"));
  
    else if (!file.isDir()) {
      *file_count = *file_count + 1;
      *byte_count = *byte_count + file.fileSize();
    }
     
    file.close();
  }
  return 0;
}

String pad = "                ";

void loop(void)
{
  uint16_t idx;
  SdFile file;
  dir_t dir;
  char name[13];
 
  String command = F("");

  while (DSERIAL.available()) DSERIAL.read();
  
  int c = 0;
  while(1) {
    if (DSERIAL.available() > 0) {
      c = DSERIAL.read();
      if ((c == 8 or c == 127) && command.length() > 0) command.remove(command.length()-1);
      if (c == '\n' || c == '\r') break;
      DSERIAL.write(c);
      if (c != 8 && c != 127) command.concat(char(c));
    } else {
      // Dylan (monte_carlo_ecm, bitflipper, etc.) -
      // This delay is required because I found that if I hard loop with DSERIAL.available,
      // in certain circumstances the Arduino never sees a new character.  Various forum posts
      // seem to confirm that a short delay is required when using this style of reading
      // from Serial
      delay(20);
    }
  }
   
  String parameter = F("");
  int parameterPos = command.indexOf(' ');
  if (parameterPos > 0) {
    parameter = command.substring(parameterPos + 1);
    command = command.substring(0, parameterPos);
  }

  command.toUpperCase();
  DSERIAL.println();
//  DSERIAL.println(command);
//  DSERIAL.println(parameter);

  if (command == F("HELP")) {
    
    help();
    
  } else if (command == F("DIR") || command == F("LS")) {
    DSERIAL.println(F("Directory Listing:"));

    sd.vwd()->rewind();
    sd.vwd()->ls(&DSERIAL, LS_SIZE);

    DSERIAL.println(F("End of Directory"));
 
  } else if (command == F("PWD")) {
    sd.vwd()->getName(name, 13);
    DSERIAL.print(F("Current working directory is "));
    DSERIAL.println(name); 
  
  }else if (command == F("CD")) {
    if(!sd.chdir(parameter.c_str(), true)) {
      DSERIAL.print(F("Directory "));
      DSERIAL.print(parameter);
      DSERIAL.println(F(" not found"));
    } else {
      DSERIAL.print(F("Current directory changed to "));
      DSERIAL.println(parameter);
    }
  } else if (command == F("DEL") || command == F("RM")) {
    if (!sd.remove(parameter.c_str())) {
      DSERIAL.print(F("Failed to delete file "));
      DSERIAL.println(parameter);
    } else {
      DSERIAL.print(F("File "));
      DSERIAL.print(parameter);
      DSERIAL.println(F(" deleted"));
    }
       
  } else if (command == F("MD") || command == F("MKDIR")) {
    if (!sd.mkdir(parameter.c_str(), true)) {
      DSERIAL.print(F("Failed to create directory "));
      DSERIAL.println(parameter);
    } else {
      DSERIAL.print(F("Directory "));
      DSERIAL.print(parameter);
      DSERIAL.println(F(" created"));
    }
  } else if (command == F("RD") || command == F("RMDIR")) {
    if (!sd.rmdir(parameter.c_str())) {
      DSERIAL.print(F("Failed to remove directory "));
      DSERIAL.println(parameter);
    } else {
      DSERIAL.print(F("Directory "));
      DSERIAL.print(parameter);
      DSERIAL.println(F(" removed"));
    }
  } else if (command == F("SZ")) {
    Filcnt = 0;
    if (parameter == "*") {
      count_files(&Filesleft, &Totalleft);
      sd.vwd()->rewind();

      if (Filesleft > 0) {
        ZSERIAL.print("rz\r");
        sendzrqinit();
        delay(200);
        
        int8_t error_stop = 0;
        while (sd.vwd()->readDir(&dir) == sizeof(dir) && !error_stop) {
          // read next directory entry in current working directory
      
          // format file name
          SdFile::dirName(&dir, name);
      
          // remember position in directory
          uint32_t pos = sd.vwd()->curPosition();
           
          // open file
          if (!file.open(name, O_READ)) error(F("file.open failed"));
          
          // restore root position
          else if (!sd.vwd()->seekSet(pos)) error(F("seekSet failed"));
        
          else if (!file.isDir()) {
            if (wcs(name,&file) == ERROR) error_stop = 1;
            else delay(500);
          }
           
          file.close();
        }
        saybibi();
      } else {
        DSERIAL.println("No files found to send");
      }
    } else if (!file.open(parameter.c_str(), O_READ)) {
      DSERIAL.println(F("file.open failed"));
    } else {
      // Start the ZMODEM transfer
      ZSERIAL.print("rz\r");
      sendzrqinit();
      delay(200);
      wcs(parameter.c_str(),&file);
      saybibi();
      file.close();
    }
  } else if (command == F("RZ")) {
//    DSERIAL.println(F("Receiving file..."));
    if (wcreceive(0, 0)) {
      DSERIAL.println(F("zmodem transfer failed"));
    } else {
      DSERIAL.println(F("zmodem transfer successful"));
    }
    fout.flush();
    fout.sync();
    fout.close();

  }
}



