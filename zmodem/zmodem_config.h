#ifndef ZMODEM_CONFIG_H
#define ZMODEM_CONFIG_H

// Dylan (monte_carlo_ecm, bitflipper, etc.) - The SparkFun MP3 shield (which contains an SDCard)
// doesn't operate properly with the SDFat library (SPI related?) unless the MP3 library is
// initialized as well.  If you are using a SparkFun MP3 shield as your SDCard interface,
// the following macro must be defined.  Otherwise, comment it out.

//#define SFMP3_SHIELD

//#define ZMODEM_SPEED 115200
// Dylan (monte_carlo_ecm, bitflipper, etc.) - I couldn't get rz to operate reliably above 57600
#define ZMODEM_SPEED 57600


#endif

