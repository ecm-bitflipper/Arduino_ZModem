#ifndef ZMODEM_ZM_h
#define ZMODEM_ZM_H

#define VERSION Progname

extern int Rxtimeout;            /* Tenths of seconds to wait for something */

/* Globals used by ZMODEM functions */
extern int Rxframeind;         /* ZBIN ZBIN32, or ZHEX type of frame received */
extern int Rxtype;             /* Type of header received */
extern int Rxcount;            /* Count of data bytes received */
extern char Rxhdr[4];          /* Received header */
extern char Txhdr[4];          /* Transmitted header */
extern long Rxpos;             /* Received file position */
extern long Txpos;             /* Transmitted file position */
extern int Txfcs32;            /* TRUE means send binary frames with 32 bit FCS */
extern int Crc32t;             /* Display flag indicating 32 bit CRC being sent */
extern int Crc32;              /* Display flag indicating 32 bit CRC being received */
extern int Znulls;             /* Number of nulls to send at beginning of ZDATA hdr */
extern char Attn[ZATTNLEN+1];  /* Attention string rx sends to tx on err */

extern int lastsent;        /* Last char we sent */
extern int Not8bit;         /* Seven bits seen on header */

extern char *frametypes[];

extern uint32_t Baudrate;
#define xsendline(c) sendline(c)
//int readline(int timeout);

#define OK 0
#define FALSE 0
#define TRUE 1
#undef ERROR
#define ERROR (-1)

#ifndef ARDUINO
#define zperr(a, ... )
#else
/*
#define WHERESTR "[FILE : %s, FUNC : %s, LINE : %d]: "
#define WHEREARG __FILE__,__func__,__LINE__
#define DEBUG(...)  {char s[256]; sprintf(s, __VA_ARGS__); DSERIAL.println(s);}
#define zperr(_fmt, ...) DEBUG(WHERESTR _fmt, WHEREARG,__VA_ARGS__)
*/
//#define zperr(...) {char s[256]; sprintf(s, __VA_ARGS__); DSERIAL.println(s);}
#define zperr(...)
#endif

void bttyout(int c);

extern int Zmodem;           /* ZMODEM protocol requested */
extern int Verbose;
extern char zconv;             /* ZMODEM file conversion request */
extern char zmanag;            /* ZMODEM file management request */
extern char ztrans;            /* ZMODEM file transport request */
extern int Zctlesc;            /* Encode control characters */
extern int Zrwindow;    /* RX window size (controls garbage count) */
extern int Nozmodem;
extern char Lzmanag;
//extern int Restricted;
extern int Quiet;
extern int Eofseen;

extern int firstsec;
extern char Lastrx;
extern char Crcflg;
extern int errors;
// This is declared in the main sketch .ino
extern char *Progname;
#endif
