/*
	Keep everything for ANSI prototypes.
From: http://stackoverflow.com/questions/2607853/why-prototype-is-used-header-files
*/

#ifndef ZMODEM_FIXES_H
#define ZMODEM_FIXES_H


// >>> Set these first three defines

// Serial output for debugging info
#define DSERIAL Serial

// The Serial port for the Zmodem connection
// must not be the same as DSERIAL unless all
// debugging output to DSERIAL is removed
//#define ZSERIAL Serial3
#define ZSERIAL Serial


////////////////////////////////////////////////////////


#define _PROTOTYPE(function, params) function params

#include <SdFat.h>

extern SdFat sd;

#include <string.h>

// Dylan (monte_carlo_ecm, bitflipper, etc.) - changed serial read/write to macros to try to squeeze 
// out higher speed

#define READCHECK
#define TYPICAL_SERIAL_TIMEOUT 1200

#define readline(timeout) ({ byte _c; ZSERIAL.readBytes(&_c, 1) > 0 ? _c : TIMEOUT; })
int zdlread2(int);
#define zdlread(void) ({ int _z; ((_z = readline(Rxtimeout)) & 0140) ? _z : zdlread2(_z); })
#define sendline(_c) ZSERIAL.write(char(_c))
#define zsendline(_z) ({ (_z & 0140 ) ? sendline(_z) : zsendline2(_z); })

void sendzrqinit(void);
int wctxpn(const char *name);
#define ARDUINO_RECV
//int wcrx(void);
int wcreceive(int argc, char **argp);

extern int Filcnt;

#define time_t int
#define register int

// If this is not defined the default is 1024.
// It must be a power of 2
#define TXBSIZE 1024


#define sleep(x) delay((x)*1000L)
#define signal(x,y)

// Handle the calls to exit - one has SS_NORMAL
#define SS_NORMAL 0
#define exit(n)

// For now, evaluate it to zero so that it does not
// enter the "if" statement's clause
#define setjmp(j) (0)

#define printf(s, ... ) DSERIAL.println(s);

// fseek(in, Rxpos, 0)
//#define fseek(fd, pos, rel) sdfile->seekSet(pos)
//#define fclose(c)

// ignore calls to mode() function in rbsb.cpp
#define mode(a)

#define sendbrk()

extern int Fromcu;
void purgeline(void);

#ifndef UNSL
#define UNSL unsigned
#endif

void flushmo(void);

#endif
