#define PUBDIR "/usr/spool/uucppublic"

// ATM remove all the rz code
//#define TEST_REMOVE
#ifndef TEST_REMOVE

/*% cc -compat -M2 -Ox -K -i -DMD -DOMEN % -o rz; size rz;
 <-xtx-*> cc386 -Ox -DMD -DOMEN -DSEGMENTS=8 rz.c -o $B/rz;  size $B/rz
 *
 * rz.c By Chuck Forsberg
 *
 *      cc -O rz.c -o rz                USG (3.0) Unix
 *      cc -O -DV7  rz.c -o rz          Unix V7, BSD 2.8 - 4.3
 *
 *      ln rz rb;  ln rz rx                     For either system
 *
 *      ln rz /usr/bin/rzrmail          For remote mail.  Make this the
 *                                      login shell. rzrmail then calls
 *                                      rmail(1) to deliver mail.
 *
 * To compile on VMS:
 *
 *      define LNK$LIBRARY   SYS$LIBRARY:VAXCRTL.OLB
 *      cc rz.c
 *      cc vvmodem.c
 *      link rz,vvmodem
 *      rz :== $disk:[username.subdir]rz.exe
 *
 *
 *  Unix is a trademark of Western Electric Company
 *
 * A program for Unix to receive files and commands from computers running
 *  Professional-YAM, PowerCom, YAM, IMP, or programs supporting XMODEM.
 *  rz uses Unix buffered input to reduce wasted CPU time.
 *
 * Iff the program is invoked by rzCOMMAND, output is piped to 
 * "COMMAND filename"  (Unix only)
 *
 *  Some systems (Venix, Coherent, Regulus) may not support tty raw mode
 *  read(2) the same way as Unix. ONEREAD must be defined to force one
 *  character reads for these systems. Added 7-01-84 CAF
 *
 *  Alarm signal handling changed to work with 4.2 BSD 7-15-84 CAF 
 *
 *  BIX added 6-30-87 to support BIX(TM) upload protocol used by the
 *  Byte Information Exchange.
 *
 *  NFGVMIN Updated 2-18-87 CAF for Xenix systems where c_cc[VMIN]
 *  doesn't work properly (even though it compiles without error!),
 *
 *  SEGMENTS=n added 2-21-88 as a model for CP/M programs
 *    for CP/M-80 systems that cannot overlap modem and disk I/O.
 *
 *  VMS flavor hacks begin with rz version 2.00
 *
 *  -DMD may be added to compiler command line to compile in
 *    Directory-creating routines from Public Domain TAR by John Gilmore
 *
 *  HOWMANY may be tuned for best performance
 *
 *  USG UNIX (3.0) ioctl conventions courtesy  Jeff Martin
 */

#ifndef ARDUINO

#include <sys/types.h>

#ifdef vax11c
#include <types.h>
#include <stat.h>
#define LOGFILE "rzlog.tmp"
#define OS "VMS"
#define BUFREAD
extern int errno;
#define SS_NORMAL SS$_NORMAL
#else
/* Not vax11c */
#define SS_NORMAL 0
#define LOGFILE "/tmp/rzlog"
#endif

#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

#include "zm.c"
#else
#include <stdio.h>

#define xsendline(c) sendline(c)
#include <setjmp.h>
#include "zmodem_fixes.h"
#include "zmodem.h"
#include "zmodem_zm.h"
#include "zmodem_crc16.cpp"
#endif

#define xsendline(c) sendline(c)

_PROTOTYPE(long getfree , (void));
_PROTOTYPE(void alrm , (int sig ));
_PROTOTYPE(int main , (int argc , char *argv []));
_PROTOTYPE(int usage , (void));
_PROTOTYPE(int wcreceive , (int argc , char **argp ));
_PROTOTYPE(int wcrxpn , (char *rpn ));
_PROTOTYPE(int wcrx , ());
_PROTOTYPE(int wcgetsec , (char *rxbuf , int maxtime ));
//_PROTOTYPE(int readline , (int timeout ));
_PROTOTYPE(void purgeline , (void));
_PROTOTYPE(int procheader , (char *name ));
_PROTOTYPE(int make_dirs , (char *pathname ));
_PROTOTYPE(int makedir , (char *dpath , int dmode ));
_PROTOTYPE(int putsec , (char *buf , int n ));
//_PROTOTYPE(void sendline , (int c ));
_PROTOTYPE(void flushmo , (void));
_PROTOTYPE(void uncaps , (char *s ));
_PROTOTYPE(int IsAnyLower , (char *s ));
_PROTOTYPE(char *substr , (char *s , char *t ));

// Pete (El Supremo)
//void zperr();


_PROTOTYPE(void canit , (void));
_PROTOTYPE(void report , (int sct ));
_PROTOTYPE(void chkinvok , (char *s ));
_PROTOTYPE(int checkpath , (char *name ));
_PROTOTYPE(int tryz , (void));
_PROTOTYPE(int rzfiles , (void));
_PROTOTYPE(int rzfile , (void));
_PROTOTYPE(void zmputs , (char *s ));
_PROTOTYPE(int closeit , (void));
_PROTOTYPE(void ackbibi , (void));
_PROTOTYPE(void bttyout , (int c ));
_PROTOTYPE(int sys2 , (char *s ));
_PROTOTYPE(void exec2 , (char *s ));

/*
 * Max value for HOWMANY is 255.
 *   A larger value reduces system overhead but may evoke kernel bugs.
 *   133 corresponds to an XMODEM/CRC sector
 */
#ifndef HOWMANY
#define HOWMANY 133
#endif



#define WCEOT (-10)
#define PATHLEN 257     /* ready for 4.2 bsd ? */
#define UNIXFILE 0xF000 /* The S_IFMT file mask bit for stat */





#ifndef ARDUINO
#ifdef vax11c
#include "vrzsz.c"      /* most of the system dependent stuff here */
#else
#include "rbsb.c"       /* most of the system dependent stuff here */
#endif

#include "crctab.c"
#endif

#ifndef ARDUINO
FILE *fout;
#else
SdFile fout;
#endif

// Dylan (monte_carlo_ecm, bitflipper, etc.) - Moved this to a global variable to enable
// crash recovery (continuation of partial file transfer)
long rxbytes;

/*
 * Routine to calculate the free bytes on the current file system
 *  ~0 means many free bytes (unknown)
 */
long getfree(void)
{
  return(~0L);    /* many free bytes ... */
}





#ifdef ONEREAD
/* Sorry, Regulus and some others don't work right in raw mode! */
int Readnum = 1;        /* Number of bytes to ask for in read() from modem */
#else
int Readnum = HOWMANY;  /* Number of bytes to ask for in read() from modem */
#endif

#define DEFBYTL 2000000000L     /* default rx file size */
long Bytesleft;         /* number of bytes of incoming file left */
long Modtime;           /* Unix style mod time for incoming file */
int Filemode;           /* Unix style mode for incoming file */
char Pathname[PATHLEN];
//char *Progname;         /* the name by which we were called */

int Batch=0;
int Topipe=0;
int MakeLCPathname=TRUE;        /* make received pathname lower case */


int Nflag = 0;          /* Don't really transfer files */
int Rxclob=FALSE;       /* Clobber existing file */
int Rxbinary=FALSE;     /* receive all files in bin mode */
int Rxascii=FALSE;      /* receive files in ascii (translate) mode */
int Thisbinary;         /* current file is to be received in bin mode */
int Blklen;             /* record length of received packets */

#ifdef SEGMENTS
int chinseg = 0;        /* Number of characters received in this data seg */
char secbuf[1+(SEGMENTS+1)*1024];
#else
char secbuf[1025];
#define SECBUF_LEN 1024
#endif


char linbuf[HOWMANY];
int Lleft=0;            /* number of characters in linbuf */
time_t timep[2];




//PEL
//jmp_buf tohere;         /* For the interrupt on RX timeout */



int tryzhdrtype=ZRINIT; /* Header type to send corresponding to Last rx close */

#ifdef ARDUINO_RECV
/*
void alrm(int sig)
{
  longjmp(tohere, -1);
}
*/

#ifndef ARDUINO
/* called by signal interrupt or terminate to clean things up */
void bibi(int n)
{
  if (Zmodem)
    zmputs(Attn);
  canit(); 
  mode(0);
  fprintf(stderr, "rz: caught signal %d; exiting\n", n);
  cucheck();
  exit(128+n);
}
#endif


/*
 *  Debugging information output interface routine
 */
/* VARARGS1 */
/*void vfile(char *f, char *a, char *b, char *c)
{
  if (Verbose > 2) {
    fprintf(stderr, f, a, b, c);
    fprintf(stderr, "\n");
  }
}*/

/*
 * Let's receive something already.
 */

char *rbmsg =
"%s ready. To begin transfer, type \"%s file ...\" to your modem program\r\n\n";

int wcreceive(int argc, char **argp)
//int argc;
//char **argp;
{
  register c;

  if (Batch || argc==0) {
    Crcflg=1;
    if ( !Quiet)
      fprintf(stderr, rbmsg, Progname, Nozmodem?"sb":"sz");
    if (c=tryz()) {
      if (c == ZCOMPL) {
        fout.close();
        return OK;
      }
      if (c == ERROR) {
//DSERIAL.println(F("fubar 1"));
        goto fubar;
      }
      c = rzfiles();
      if (c) {
//DSERIAL.println(F("fubar 2"));
        goto fubar;
      }
    } 
    else {
      for (;;) {
        if (wcrxpn(secbuf)== ERROR) {
//DSERIAL.println(F("fubar 3"));
          goto fubar;
        }
        if (secbuf[0]==0)
          return OK;
        if (procheader(secbuf) == ERROR) {
//DSERIAL.println(F("fubar 4"));
          goto fubar;
        }
        if (wcrx()==ERROR) {
//DSERIAL.println(F("fubar 5"));          
          goto fubar;
        }
      }
    }
  } 
  else {
    Bytesleft = DEFBYTL; 
    Filemode = 0; 
    Modtime = 0L;

    procheader(""); 
    strcpy(Pathname, *argp); 
//    if (checkpath(Pathname)) {
//      canit();
//      return ERROR;
//    }
//DSERIAL.print("rz: ready to receive ");
//DSERIAL.println(Pathname);
#ifndef ARDUINO
    if ((fout=fopen(Pathname, "w")) == NULL)
#else
    if (!fout.open(Pathname, O_WRITE | O_CREAT | O_AT_END))
#endif
      return ERROR;
    rxbytes = fout.fileSize();
    
    if (wcrx()==ERROR) {
DSERIAL.println(F("fubar 6"));
      goto fubar;
    }
  }
  fout.close();
  return OK;
fubar:
  canit();
#ifndef ARDUINO
#ifndef vax11c
  if (Topipe && fout) {
    pclose(fout);  
    return ERROR;
  }
#endif
  if (fout)
    fclose(fout);
#ifndef vax11c
  if (Restricted) {
    unlink(Pathname);
    fprintf(stderr, F("\r\nrz: %s removed.\r\n"), Pathname);
  }
#endif
#else
  fout.flush();
  fout.sync();
  fout.close();
#endif
  return ERROR;
}

int Firstsec;
/*
 * Fetch a pathname from the other end as a C ctyle ASCIZ string.
 * Length is indeterminate as long as less than Blklen
 * A null string represents no more files (YMODEM)
 */
int wcrxpn(char *rpn)
/* receive a pathname */
{
  register c;

#ifdef NFGVMIN
  readline(1);
#else
  purgeline();
#endif

et_tu:
  Firstsec=TRUE;  
  Eofseen=FALSE;
  sendline(Crcflg?WANTCRC:NAK);
  Lleft=0;        /* Do read next time ... */
  while ((c = wcgetsec(rpn, 100)) != 0) {
    if (c == WCEOT) {
      zperr( "Pathname fetch returned %d", c);
      sendline(ACK);
      Lleft=0;        /* Do read next time ... */
      readline(1);
      goto et_tu;
    }
    return ERROR;

  
  }
  sendline(ACK);
  return OK;
}

/*
 * Adapted from CMODEM13.C, written by
 * Jack M. Wierda and Roderick W. Hart
 */

int wcrx()
{
  int sectnum, sectcurr;
  char sendchar;
  int cblklen;                    /* bytes to dump this block */

  Firstsec=TRUE;
  sectnum=0; 
  Eofseen=FALSE;
  sendchar=Crcflg?WANTCRC:NAK;

  for (;;) {
    sendline(sendchar);     /* send it now, we're ready! */
    Lleft=0;        /* Do read next time ... */
    sectcurr=wcgetsec(secbuf, (sectnum&0177)?50:130);
    report(sectcurr);
    if (sectcurr==((sectnum+1) &0377)) {
      sectnum++;
      cblklen = Bytesleft>Blklen ? Blklen:Bytesleft;
      if (putsec(secbuf, cblklen)==ERROR)
        return ERROR;
      if ((Bytesleft-=cblklen) < 0)
        Bytesleft = 0;
      sendchar=ACK;
    }
    else if (sectcurr==(sectnum&0377)) {
      zperr( "Received dup Sector");
      sendchar=ACK;
    }
    else if (sectcurr==WCEOT) {
      if (closeit())
        return ERROR;
      sendline(ACK);
      Lleft=0;        /* Do read next time ... */
      return OK;
    }
    else if (sectcurr==ERROR)
      return ERROR;
    else {
      zperr( "Sync Error");
      return ERROR;
    }
  }
}

/*
 * Wcgetsec fetches a Ward Christensen type sector.
 * Returns sector number encountered or ERROR if valid sector not received,
 * or CAN CAN received
 * or WCEOT if eot sector
 * time is timeout for first char, set to 4 seconds thereafter
 ***************** NO ACK IS SENT IF SECTOR IS RECEIVED OK **************
 *    (Caller must do that when he is good and ready to get next sector)
 */

int wcgetsec(char *rxbuf,int maxtime)
{
  int checksum, wcj, firstch;
  unsigned short oldcrc;
  char *p;
  int sectcurr;

  for (Lastrx=errors=0; errors<Rx_RETRYMAX; errors++) {

    if ((firstch=readline(maxtime))==STX) {
      Blklen=1024; 
      goto get2;
    }
    if (firstch==SOH) {
      Blklen=128;
get2:
      sectcurr=readline(1);
      if ((sectcurr+(oldcrc=readline(1)))==0377) {
        oldcrc=checksum=0;
        for (p=rxbuf,wcj=Blklen; --wcj>=0; ) {
          if ((firstch=readline(1)) < 0) {
//DSERIAL.println(F("bilge 1"));
            goto bilge;
          }
          oldcrc=updcrc(firstch, oldcrc);
          checksum += (*p++ = firstch);
        }
        if ((firstch=readline(1)) < 0) {
//DSERIAL.println(F("bilge 2"));

          goto bilge;
        }
        if (Crcflg) {
          oldcrc=updcrc(firstch, oldcrc);
          if ((firstch=readline(1)) < 0) {
//DSERIAL.println(F("bilge 3"));

            goto bilge;
          }
          oldcrc=updcrc(firstch, oldcrc);
          if (oldcrc & 0xFFFF) {
            zperr( "CRC");
          } else {
            Firstsec=FALSE;
            return sectcurr;
          }
        }
        else if (((checksum-firstch)&0377)==0) {
          Firstsec=FALSE;
          return sectcurr;
        }
        else
          zperr( "Checksum");
      }
      else
        zperr("Sector number garbled");
    }
    /* make sure eot really is eot and not just mixmash */
#ifdef NFGVMIN
    else if (firstch==EOT && readline(1)==TIMEOUT)
      return WCEOT;
#else
    else if (firstch==EOT && Lleft==0)
      return WCEOT;
#endif
    else if (firstch==CAN) {
      if (Lastrx==CAN) {
        zperr( "Sender CANcelled");
        return ERROR;
      } 
      else {
        Lastrx=CAN;
        continue;
      }
    }
    else if (firstch==TIMEOUT) {
      if (Firstsec)
        goto humbug;
bilge:
      zperr( "TIMEOUT");
    }
    else
      zperr( "Got 0%o sector header", firstch);

humbug:
    Lastrx=0;
    while(readline(1)!=TIMEOUT)
      ;
    if (Firstsec) {
      sendline(Crcflg?WANTCRC:NAK);
      Lleft=0;        /* Do read next time ... */
    } 
    else {
      maxtime=40; 
      sendline(NAK);
      Lleft=0;        /* Do read next time ... */
    }
  }
  /* try to stop the bubble machine. */
  canit();
  return ERROR;
}
#endif


#ifndef NOTDEF

/*
 * Process incoming file information header
 */
int procheader(char *name)
{
  char *openmode, *p;

  /* set default parameters and overrides */
  openmode = "w";
  Thisbinary = (!Rxascii) || Rxbinary;
  if (Lzmanag)
    zmanag = Lzmanag;

  /*
         *  Process ZMODEM remote file management requests
   */
  if (!Rxbinary && zconv == ZCNL) /* Remote ASCII override */
    Thisbinary = 0;
  if (zconv == ZCBIN)     /* Remote Binary override */
    Thisbinary = TRUE;
  else if (zmanag == ZMAPND)
    openmode = "a";

#ifndef BIX
  /* Check for existing file */
#ifndef ARDUINO
  if (!Rxclob && (zmanag&ZMMASK) != ZMCLOB && (fout=fopen(name, "r"))) {
    fclose(fout);  
    return ERROR;
  }
#else
//  if (!Rxclob && (zmanag&ZMMASK) != ZMCLOB && sd.exists(name)) {
//    return ERROR;
//  }
#endif
#endif

  Bytesleft = DEFBYTL; 
  Filemode = 0; 
  Modtime = 0L;

  p = name + 1 + strlen(name);
  if (*p) {       /* file coming from Unix or DOS system */
    sscanf(p, "%ld%lo%o", &Bytesleft, &Modtime, &Filemode);
#ifndef vax11c
    if (Filemode & UNIXFILE)
      ++Thisbinary;
#endif
    if (Verbose) {
      fprintf(stderr,  "\nIncoming: %s %ld %lo %o\n",
      name, Bytesleft, Modtime, Filemode);
    }
  }

#ifdef BIX
  if ((fout=fopen(F("scratchpad"), openmode)) == NULL)
    return ERROR;
  return OK;
#else

  else {          /* File coming from CP/M system */
    for (p=name; *p; ++p)           /* change / to _ */
      if ( *p == '/')
        *p = '_';

    if ( *--p == '.')               /* zap trailing period */
      *p = 0;
  }

#ifndef vax11c
  if (!Zmodem && MakeLCPathname && !IsAnyLower(name)
    && !(Filemode&UNIXFILE))
    uncaps(name);
#endif
  if (Topipe > 0) {
    sprintf(Pathname, "%s %s", Progname+2, name);
    if (Verbose)
      fprintf(stderr,  "Topipe: %s %s\n",
      Pathname, Thisbinary?"BIN":"ASCII");
#ifndef vax11c
#ifndef ARDUINO
    if ((fout=popen(Pathname, "w")) == NULL)
#else
    if (!fout.open(Pathname, O_WRITE | O_CREAT | O_AT_END))
#endif
      return ERROR;

    rxbytes = fout.fileSize();
#endif
  } 
  else {
    strcpy(Pathname, name);
    if (Verbose) {
      fprintf(stderr,  "Receiving %s %s %s\n",
      name, Thisbinary?"BIN":"ASCII", openmode);
    }
//    if (checkpath(name)) {
//      canit();
//      return ERROR;
//    }
    if (Nflag)
      name = "/dev/null";
#ifndef vax11c
#ifdef OMEN
    if (name[0] == '!' || name[0] == '|') {
      if ( !(fout = popen(name+1, "w"))) {
        return ERROR;
      }
      Topipe = -1;  
      return(OK);
    }
#endif
#endif
#ifdef MD
    fout = fopen(name, openmode);
    if ( !fout)
      if (make_dirs(name))
        fout = fopen(name, openmode);
#else
#ifndef ARDUINO
    fout = fopen(name, openmode);
#else
    fout.open(name, O_WRITE | O_CREAT | O_AT_END);
    rxbytes = fout.fileSize();

#endif
#endif
#ifndef ARDUINO
    if ( !fout)
#else
    if (!fout.isOpen())
#endif
      return ERROR;
  }
  return OK;
#endif /* BIX */
}

#ifdef MD
/*
 *  Directory-creating routines from Public Domain TAR by John Gilmore
 */

/*
 * After a file/link/symlink/dir creation has failed, see if
 * it's because some required directory was not present, and if
 * so, create all required dirs.
 */
int make_dirs(char *pathname)
{
  register char *p;               /* Points into path */
  int madeone = 0;                /* Did we do anything yet? */
  int save_errno = errno;         /* Remember caller's errno */
  char *strchr();

  if (errno != ENOENT)
    return 0;               /* Not our problem */

  for (p = strchr(pathname, '/'); p != NULL; p = strchr(p+1, '/')) {
    /* Avoid mkdir of empty string, if leading or double '/' */
    if (p == pathname || p[-1] == '/')
      continue;
    /* Avoid mkdir where last part of path is '.' */
    if (p[-1] == '.' && (p == pathname+1 || p[-2] == '/'))
      continue;
    *p = 0;                         /* Truncate the path there */
    if ( !makedir(pathname, 0777)) {        /* Try to create it as a dir */
      vfile(F("Made directory %s\n"), pathname);
      madeone++;              /* Remember if we made one */
      *p = '/';
      continue;
    }
    *p = '/';
    if (errno == EEXIST)            /* Directory already exists */
      continue;
    /*
                 * Some other error in the makedir.  We return to the caller.
     */
    break;
  }
  errno = save_errno;             /* Restore caller's errno */
  return madeone;                 /* Tell them to retry if we made one */
}

#if (MD != 2)
#define TERM_SIGNAL(status)     ((status) & 0x7F)
#define TERM_COREDUMP(status)   (((status) & 0x80) != 0)
#define TERM_VALUE(status)      ((status) >> 8)
/*
 * Make a directory.  Compatible with the mkdir() system call on 4.2BSD.
 */
int makedir(char *dpath,int dmode)
{
  int cpid, status;
  struct stat statbuf;

  if (stat(dpath,&statbuf) == 0) {
    errno = EEXIST;         /* Stat worked, so it already exists */
    return -1;
  }

  /* If stat fails for a reason other than non-existence, return error */
  if (errno != ENOENT) return -1; 

  switch (cpid = fork()) {

  case -1:                        /* Error in fork() */
    return(-1);             /* Errno is set already */

  case 0:                         /* Child process */
    /*
                 * Cheap hack to set mode of new directory.  Since this
     * child process is going away anyway, we zap its umask.
     * FIXME, this won't suffice to set SUID, SGID, etc. on this
     * directory.  Does anybody care?
     */
    status = umask(0);      /* Get current umask */
    status = umask(status | (0777 & ~dmode)); /* Set for mkdir */
    execl("/bin/mkdir", "mkdir", dpath, (char *)0);
    _exit(-1);              /* Can't exec /bin/mkdir */

  default:                        /* Parent process */
    while (cpid != wait(&status)) ; /* Wait for kid to finish */
  }

  if (TERM_SIGNAL(status) != 0 || TERM_VALUE(status) != 0) {
    errno = EIO;            /* We don't know why, but */
    return -1;              /* /bin/mkdir failed */
  }

  return 0;
}
#endif /* MD != 2 */
#endif /* MD */

/*
 * Putsec writes the n characters of buf to receive file fout.
 *  If not in binary mode, carriage returns, and all characters
 *  starting with CPMEOF are discarded.
 */
int putsec(char *buf,int n)
{
  if (n == 0)
    return OK;
  if (Thisbinary) {
#ifndef ARDUINO
    for (char *p=buf; --n>=0; )
      putc( *p++, fout);
#else
    fout.write(buf, n);
//    for (p=buf; --n>=0; )
//      fout.write(*p++);

#endif
  }
  else {
    if (Eofseen)
      return OK;
    for (char *p=buf; --n>=0; ++p ) {
      if ( *p == '\r')
        continue;
      if (*p == CPMEOF) {
        Eofseen=TRUE; 
        return OK;
      }
#ifndef ARDUINO      
      putc(*p ,fout);
#else
      fout.write(*p);
#endif
    }
  }
  return OK;
}
#endif


/* make string s lower case */
void uncaps(char *s)
{
  for ( ; *s; ++s)
    if (isupper(*s))
      *s = tolower(*s);
}
/*
 * IsAnyLower returns TRUE if string s has lower case letters.
 */
int IsAnyLower(char *s)
{
  for ( ; *s; ++s)
    if (islower(*s))
      return TRUE;
  return FALSE;
}

/*
 * substr(string, token) searches for token in string s
 * returns pointer to token within string if found, NULL otherwise
 */
char *substr(char *s, char *t)
{
  char *ss,*tt;
  /* search for first char of token */
  for (ss=s; *s; s++)
    if (*s == *t)
      /* compare token with substring */
      for (ss=s,tt=t; ;) {
        if (*tt == 0)
          return s;
        if (*ss++ != *tt++)
          break;
      }
  return (char *)NULL;
}



void report(int sct)
{
  if (Verbose>1)
    fprintf(stderr,"%03d%c",sct,sct%10? ' ' : '\r');
}

#ifndef vax11c
/*
 * If called as [-][dir/../]vrzCOMMAND set Verbose to 1
 * If called as [-][dir/../]rzCOMMAND set the pipe flag
 * If called as rb use YMODEM protocol
 */
void chkinvok(char *s)
{
  char *p;

  p = s;
  while (*p == '-')
    s = ++p;
  while (*p)
    if (*p++ == '/')
      s = p;
  if (*s == 'v') {
    Verbose=1; 
    ++s;
  }
  Progname = s;
  if (s[0]=='r' && s[1]=='z')
    Batch = TRUE;
  if (s[0]=='r' && s[1]=='b')
    Batch = Nozmodem = TRUE;
  if (s[2] && s[0]=='r' && s[1]=='b')
    Topipe = 1;
  if (s[2] && s[0]=='r' && s[1]=='z')
    Topipe = 1;
}
#endif

/*
 * Totalitarian Communist pathname processing
Dylan (monte_carlo_ecm, bitflipper, etc.) - This really isn't necessary
since we can open the file for append (crash recovery) and security is not
relevant in a closed environment - removing
 */
/* int checkpath(char *name)
{
  if (Restricted) {
#ifndef ARDUINO    
    if (fopen(name, "r") != NULL) {
#else
    if (sd.exists(name)) {
#endif
      canit();
      fprintf(stderr, (char *)"\r\nrz: %s exists\n", name);
      return ERROR;
    }
    // restrict pathnames to current tree or uucppublic
    if ( substr(name, (char *)"../")
      || (name[0]== '/' && strncmp(name, PUBDIR, strlen(PUBDIR))) ) {
      canit();
      fprintf(stderr,(char *)"\r\nrz:\tSecurity Violation\r\n");
      return ERROR;
    }
  }
  return 0;
}
*/

/*
 * Initialize for Zmodem receive attempt, try to activate Zmodem sender
 *  Handles ZSINIT frame
 *  Return ZFILE if Zmodem filename received, -1 on error,
 *   ZCOMPL if transaction finished,  else 0
 */
int tryz(void)
{
  int c, n;
  int cmdzack1flg;

//DSERIAL.println(F("Entering tryz"));

  if (Nozmodem)           /* Check for "rb" program name */
    return 0;

  tryzhdrtype=ZRINIT;

  for (n=Zmodem?15:5; --n>=0; ) {
    /* Set buffer length (0) and capability flags */
#ifdef SEGMENTS
    stohdr(SEGMENTS*1024L);
#else
#ifndef ARDUINO
    stohdr(0L);
#else
// This unfortunate piece is a limiting factor.  This parameter is supposed to control
// the maximum buffer size that the sender expects us to have.  It seems most ZModem send implementations
// ignore it, with Hyperterminal being the only exception I can find.  Without setting this, even
// Hyperterminal outstrips the Arduino's speed and buffer (64 bytes) at 57600 baud. 
    stohdr(1024L);

#endif

#endif
#ifdef CANBREAK
    Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO|CANBRK;
#else
    Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO;
#endif

    if (Zctlesc)
      Txhdr[ZF0] |= TESCCTL;
    zshhdr(tryzhdrtype, Txhdr);
    if (tryzhdrtype == ZSKIP)       /* Don't skip too far */
      tryzhdrtype = ZRINIT;   /* CAF 8-21-87 */
again:
    switch (zgethdr(Rxhdr, 0)) {
    case ZRQINIT:
//DSERIAL.println(F("tryz got ZRQINIT"));
      continue;
    case ZEOF:
//DSERIAL.println(F("tryz got ZEOF"));
      continue;
    case TIMEOUT:
//DSERIAL.println(F("tryz got TIMEOUT"));
      continue;
    case ZFILE:
//DSERIAL.println(F("tryz got ZFILE"));

      zconv = Rxhdr[ZF0];
      zmanag = Rxhdr[ZF1];
      ztrans = Rxhdr[ZF2];
      tryzhdrtype = ZRINIT;
      c = zrdata(secbuf, SECBUF_LEN);
      mode(3);
      if (c == GOTCRCW)
        return ZFILE;
      zshhdr(ZNAK, Txhdr);
      goto again;
    case ZSINIT:
//DSERIAL.println(F("tryz got ZSINIT"));

      Zctlesc = TESCCTL & Rxhdr[ZF0];
      if (zrdata(Attn, ZATTNLEN) == GOTCRCW) {
        stohdr(1L);
        zshhdr(ZACK, Txhdr);
        goto again;
      }
      zshhdr(ZNAK, Txhdr);
      goto again;
    case ZFREECNT:
      stohdr(getfree());
      zshhdr(ZACK, Txhdr);
      goto again;
    case ZCOMMAND:
#ifdef vax11c
      return ERROR;
#else
//DSERIAL.println(F("tryz got ZCOMMAND"));

      cmdzack1flg = Rxhdr[ZF0];
      if (zrdata(secbuf, SECBUF_LEN) == GOTCRCW) {
        if (cmdzack1flg & ZCACK1)
          stohdr(0L);
        else
          stohdr((long)sys2(secbuf));
        purgeline();    /* dump impatient questions */
        do {
          zshhdr(ZCOMPL, Txhdr);
        }
        while (++errors<20 && zgethdr(Rxhdr,1) != ZFIN);
        ackbibi();
        if (cmdzack1flg & ZCACK1)
          exec2(secbuf);
        return ZCOMPL;
      }
      zshhdr(ZNAK, Txhdr); 
      goto again;
#endif
    case ZCOMPL:
//DSERIAL.println(F("tryz got ZCOMPL"));

      goto again;
    default:
//DSERIAL.println(F("tryz got default"));
    
      continue;
    case ZFIN:
//DSERIAL.println(F("tryz got ZFIN"));

      ackbibi(); 
      return ZCOMPL;
    case ZCAN:
//DSERIAL.println(F("tryz got ZCAN"));

      return ERROR;
    }
  }
  return 0;
}

/*
 * Receive 1 or more files with ZMODEM protocol
 */
int rzfiles(void)
{
  int c;

  for (;;) {
    switch (c = rzfile()) {
    case ZEOF:
    case ZSKIP:
      switch (tryz()) {
      case ZCOMPL:
        return OK;
      default:
        return ERROR;
      case ZFILE:
        break;
      }
      continue;
    default:
      return c;
    case ERROR:
      return ERROR;
    }
  }
}

/*
 * Receive a file with ZMODEM protocol
 *  Assumes file name frame is in secbuf
 */
int rzfile(void)
{
  int c, n;
//  long rxbytes;

  Eofseen=FALSE;
  if (procheader(secbuf) == ERROR) {
    return (tryzhdrtype = ZSKIP);
  }

  n = 20; 
//  rxbytes = 0l;

  for (;;) {
#ifdef SEGMENTS
    chinseg = 0;
#endif
    stohdr(rxbytes);
    zshhdr(ZRPOS, Txhdr);
nxthdr:
    switch (c = zgethdr(Rxhdr, 0)) {
    default:
      vfile(F("rzfile: zgethdr returned %d"), c);
      return ERROR;
    case ZNAK:
    case TIMEOUT:
#ifdef SEGMENTS
      putsec(secbuf, chinseg);
      chinseg = 0;
#endif
      if ( --n < 0) {
        vfile(F("rzfile: zgethdr returned %d"), c);
        return ERROR;
      }
    case ZFILE:
      zrdata(secbuf, SECBUF_LEN);
      continue;
    case ZEOF:
#ifdef SEGMENTS
      putsec(secbuf, chinseg);
      chinseg = 0;
#endif
      if (rclhdr(Rxhdr) != rxbytes) {
        /*
                                 * Ignore eof if it's at wrong place - force
         *  a timeout because the eof might have gone
         *  out before we sent our zrpos.
         */
        errors = 0;  
        goto nxthdr;
      }
      if (closeit()) {
        tryzhdrtype = ZFERR;
        vfile(F("rzfile: closeit returned <> 0"));
        return ERROR;
      }
      vfile(F("rzfile: normal EOF"));
      return c;
    case ERROR:     /* Too much garbage in header search error */
#ifdef SEGMENTS
      putsec(secbuf, chinseg);
      chinseg = 0;
#endif
      if ( --n < 0) {
        vfile(F("rzfile: zgethdr returned %d"), c);
        return ERROR;
      }
      zmputs(Attn);
      continue;
    case ZSKIP:
#ifdef SEGMENTS
      putsec(secbuf, chinseg);
      chinseg = 0;
#endif
      closeit();
      vfile(F("rzfile: Sender SKIPPED file"));
      return c;
    case ZDATA:
      if (rclhdr(Rxhdr) != rxbytes) {
        if ( --n < 0) {
          return ERROR;
        }
#ifdef SEGMENTS
        putsec(secbuf, chinseg);
        chinseg = 0;
#endif
        zmputs(Attn);  
        continue;
      }
moredata:
      if (Verbose>1)
        fprintf(stderr, "\r%7ld ZMODEM%s    ",
        rxbytes, Crc32?" CRC-32":"");
#ifdef SEGMENTS
      if (chinseg >= (1024 * SEGMENTS)) {
        putsec(secbuf, chinseg);
        chinseg = 0;
      }
      switch (c = zrdata(secbuf+chinseg, 1024))
#else
        switch (c = zrdata(secbuf, SECBUF_LEN))
#endif
        {
        case ZCAN:
#ifdef SEGMENTS
          putsec(secbuf, chinseg);
          chinseg = 0;
#endif
          vfile(F("rzfile: zgethdr returned %d"), c);
          return ERROR;
        case ERROR:     /* CRC error */
#ifdef SEGMENTS
          putsec(secbuf, chinseg);
          chinseg = 0;
#endif
          if ( --n < 0) {
            vfile(F("rzfile: zgethdr returned %d"), c);
            return ERROR;
          }
          zmputs(Attn);
          continue;
        case TIMEOUT:
#ifdef SEGMENTS
          putsec(secbuf, chinseg);
          chinseg = 0;
#endif
          if ( --n < 0) {
            vfile(F("rzfile: zgethdr returned %d"), c);
            return ERROR;
          }
          continue;
        case GOTCRCW:
          n = 20;
#ifdef SEGMENTS
          chinseg += Rxcount;
          putsec(secbuf, chinseg);
          chinseg = 0;
#else
          putsec(secbuf, Rxcount);
#endif
          rxbytes += Rxcount;
          stohdr(rxbytes);
          zshhdr(ZACK, Txhdr);
          sendline(XON);
          goto nxthdr;
        case GOTCRCQ:
          n = 20;
#ifdef SEGMENTS
          chinseg += Rxcount;
#else
          putsec(secbuf, Rxcount);
#endif
          rxbytes += Rxcount;
          stohdr(rxbytes);
          zshhdr(ZACK, Txhdr);
          goto moredata;
        case GOTCRCG:
          n = 20;
#ifdef SEGMENTS
          chinseg += Rxcount;
#else
          putsec(secbuf, Rxcount);
#endif
          rxbytes += Rxcount;
          goto moredata;
        case GOTCRCE:
          n = 20;
#ifdef SEGMENTS
          chinseg += Rxcount;
#else
          putsec(secbuf, Rxcount);
#endif
          rxbytes += Rxcount;
          goto nxthdr;
        }
    }
  }
}

/*
 * Send a string to the modem, processing for \336 (sleep 1 sec)
 *   and \335 (break signal)
 */
void zmputs(char *s)
{
  int c;

  while (*s) {
    switch (c = *s++) {
    case '\336':
#ifndef ARDUINO    
      sleep(1); 
#else
      delay(1000);
#endif
      continue;
    case '\335':
      sendbrk(); 
      continue;
    default:
      sendline(c);
    }
  }
}

/*
 * Close the receive dataset, return OK or ERROR
 */
//>>> TODO - this needs to be fixed up for Teensy/Arduino - Done now I believe
int closeit(void)
{
#ifndef ARDUINO
  time_t q;


#ifndef vax11c
  if (Topipe) {
    if (pclose(fout)) {
      return ERROR;
    }
    return OK;
  }
#endif
  if (fclose(fout)==ERROR) {
    fprintf(stderr, "file close ERROR\n");
    return ERROR;
  }
#ifndef vax11c
  if (Modtime) {
    timep[0] = time(&q);
    timep[1] = Modtime;
    utime(Pathname, (struct utimbuf *) timep);
  }
#endif

  if ((Filemode&S_IFMT) == S_IFREG)
    chmod(Pathname, (07777 & Filemode));
#else
  fout.flush();
  fout.sync();
  fout.close();

#endif
  return OK;
}

/*
 * Ack a ZFIN packet, let byegones be byegones
 */
void ackbibi(void)
{
  int n;

  vfile(F("ackbibi:"));
  Readnum = 1;
  stohdr(0L);
  for (n=3; --n>=0; ) {
    purgeline();
    zshhdr(ZFIN, Txhdr);
    switch (readline(100)) {
    case 'O':
      readline(1);    /* Discard 2nd 'O' */
      vfile(F("ackbibi complete"));
      return;
    case RCDO:
      return;
    case TIMEOUT:
    default:
      break;
    }
  }
}




#ifndef vax11c
#ifndef ARDUINO
/*
 * Strip leading ! if present, do shell escape. 
 */
int sys2(char *s)
{
  if (*s == '!')
    ++s;
  return system(s);
}
/*
 * Strip leading ! if present, do exec.
 */
void exec2(char *s)
{

}
#else
int sys2(char *s){}
void exec2(char *s) {}
#endif
#endif
/* End of rz.c */




#endif
